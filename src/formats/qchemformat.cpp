/**********************************************************************
Copyright (C) 2000-2007 by Geoffrey Hutchison
Some portions Copyright (C) 2004 by Michael Banck
Some portions Copyright (C) 2004 by Chris Morley
 
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation version 2 of the License.
 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
***********************************************************************/
#include <openbabel/babelconfig.h>

#include <openbabel/obmolecformat.h>

using namespace std;
namespace OpenBabel
{

  class QChemOutputFormat : public OBMoleculeFormat
  {
  public:
    //Register this format type ID
    QChemOutputFormat()
    {
      OBConversion::RegisterFormat("qcout",this);
    }

    virtual const char* Description() //required
    {
      return
        "Q-Chem output format\n"
        "Read Options e.g. -as\n"
        "  s  Output single bonds only\n"
        "  b  Disable bonding entirely\n\n";
    };

    virtual const char* SpecificationURL()
    {return "http://www.q-chem.com/";}; //optional

    //Flags() can return be any the following combined by | or be omitted if none apply
    // NOTREADABLE  READONEONLY  NOTWRITABLE  WRITEONEONLY
    virtual unsigned int Flags()
    {
      return READONEONLY | NOTWRITABLE;
    };

    /// The "API" interface functions
    virtual bool ReadMolecule(OBBase* pOb, OBConversion* pConv);
  };
  //***

  //Make an instance of the format class
  QChemOutputFormat theQChemOutputFormat;


  class QChemInputFormat : public OBMoleculeFormat
  {
  public:
    //Register this format type ID
    QChemInputFormat()
    {
      OBConversion::RegisterFormat("qcin",this);
    }

    virtual const char* Description() //required
    {
      return
        "Q-Chem input format\n"
        "Write Options e.g. -xk\n"
        "  k  \"keywords\" Use the specified keywords for input\n"
        "  f    <file>     Read the file specified for input keywords\n\n";
    };

    virtual const char* SpecificationURL()
    {return "http://www.q-chem.com/";}; //optional

    //Flags() can return be any the following combined by | or be omitted if none apply
    // NOTREADABLE  READONEONLY  NOTWRITABLE  WRITEONEONLY
    virtual unsigned int Flags()
    {
      return WRITEONEONLY | NOTREADABLE;
    };

    ////////////////////////////////////////////////////
    /// The "API" interface functions
    virtual bool WriteMolecule(OBBase* pOb, OBConversion* pConv);

  };

  //Make an instance of the format class
  QChemInputFormat theQChemInputFormat;

  /////////////////////////////////////////////////////////////////
  bool QChemOutputFormat::ReadMolecule(OBBase* pOb, OBConversion* pConv)
  {

    OBMol* pmol = pOb->CastAndClear<OBMol>();
    if(pmol==NULL)
      return false;

    //Define some references so we can use the old parameter names
    istream &ifs = *pConv->GetInStream();
    OBMol &mol = *pmol;
    const char* title = pConv->GetTitle();

    char buffer[BUFF_SIZE];
    string str,str1;
    double x,y,z;
    OBAtom *atom;
    //  OBInternalCoord *coord; CM
    vector<string> vs;
    vector<OBInternalCoord *> internals; // If we get a z-matrix
    int charge = 0;
    unsigned int spin = 1;
    bool hasPartialCharges = false;
    bool hasDipoleMoment = false;
    vector3 dipoleMoment;
    bool hasVibData = false;
    vector<double> frequencies;
    vector<double> intensities;
    vector< vector<vector3> > displacements;

    mol.BeginModify();

    while	(ifs.getline(buffer,BUFF_SIZE))
      {
        if(strstr(buffer,"Standard Nuclear Orientation") != NULL)
          {
            // mol.EndModify();
            mol.Clear();
            mol.BeginModify();
            ifs.getline(buffer,BUFF_SIZE);	// column headings
            ifs.getline(buffer,BUFF_SIZE);	// ---------------
            ifs.getline(buffer,BUFF_SIZE);
            tokenize(vs,buffer);
            while (vs.size() == 5)
              {
                atom = mol.NewAtom();
                atom->SetAtomicNum(etab.GetAtomicNum(vs[1].c_str()));
                x = atof((char*)vs[2].c_str());
                y = atof((char*)vs[3].c_str());
                z = atof((char*)vs[4].c_str());
                atom->SetVector(x,y,z);

                if (!ifs.getline(buffer,BUFF_SIZE))
                  break;
                tokenize(vs,buffer);
              }
          }
        else if(strstr(buffer,"Dipole Moment") != NULL)
          {
            ifs.getline(buffer,BUFF_SIZE); // actual components   X ###  Y #### Z ###
            tokenize(vs,buffer);
            if (vs.size() >= 6) 
              {
                double x, y, z;
                x = atof(vs[1].c_str());
                y = atof(vs[3].c_str());
                z = atof(vs[5].c_str());
                dipoleMoment.Set(x, y, z);
              }
            if (!ifs.getline(buffer,BUFF_SIZE)) break;
          }
        else if(strstr(buffer,"Mulliken Net Atomic Charges") != NULL)
          {
            hasPartialCharges = true;
            ifs.getline(buffer,BUFF_SIZE);	// (blank)
            ifs.getline(buffer,BUFF_SIZE);	// column headings
            ifs.getline(buffer,BUFF_SIZE);	// -----------------
            ifs.getline(buffer,BUFF_SIZE);
            tokenize(vs,buffer);
            while (vs.size() >= 3)
              {
                atom = mol.GetAtom(atoi(vs[0].c_str()));
                atom->SetPartialCharge(atof(vs[2].c_str()));
                
                if (!ifs.getline(buffer,BUFF_SIZE))
                  break;
                tokenize(vs,buffer);
              }
          }
        else if (strstr(buffer, "ISOTROPIC") != NULL 
                 && strstr(buffer, "ATOM") != NULL) // NMR suemmary
          {
            ifs.getline(buffer, BUFF_SIZE); // -------
            ifs.getline(buffer, BUFF_SIZE);
            tokenize(vs,buffer);
            while (vs.size() >= 5)
              {
                atom = mol.GetAtom(atoi(vs[2].c_str()));
                OBPairData *nmrShift = new OBPairData();
                nmrShift->SetAttribute("NMR Isotropic Shift");
                nmrShift->SetValue(vs[3]);
                atom->SetData(nmrShift);

                if (!ifs.getline(buffer, BUFF_SIZE))
                  break;
                tokenize(vs, buffer);
              }
          }
        else if(strstr(buffer,"Frequency:") != NULL)
          {
            hasVibData = true;
            // We'll only see this data once -- several modes per "section"
            // (e.g., 2 or 3 across)
            tokenize(vs,buffer);
            for(int i=1; i < vs.size(); ++i)
              frequencies.push_back(atof(vs[i].c_str()));

            ifs.getline(buffer,BUFF_SIZE);	// IR active
            ifs.getline(buffer,BUFF_SIZE);
            tokenize(vs,buffer);
            for(int i=1; i < vs.size(); ++i)
              intensities.push_back(atof(vs[i].c_str()));

            ifs.getline(buffer,BUFF_SIZE);	// Raman active
            ifs.getline(buffer,BUFF_SIZE);	// column headings
            ifs.getline(buffer,BUFF_SIZE);	// actual displacement data
            tokenize(vs, buffer);
            vector<vector3> vib1, vib2, vib3;
            double x, y, z;
            while (vs.size() > 3) {
              for (int i = 1; i < vs.size(); i += 3) {
                x = atof(vs[i].c_str());
                y = atof(vs[i+1].c_str());
                z = atof(vs[i+2].c_str());

                if (i == 1)
                  vib1.push_back(vector3(x, y, z));
                else if (i == 4)
                  vib2.push_back(vector3(x, y, z));
                else if (i == 7)
                  vib3.push_back(vector3(x, y, z));
              }

              if (!ifs.getline(buffer, BUFF_SIZE))
                break;
              tokenize(vs,buffer);
            }
            displacements.push_back(vib1);
            if (vib2.size())
              displacements.push_back(vib2);
            if (vib3.size())
              displacements.push_back(vib3);
          }

        // In principle, the final geometry in cartesians is exactly the same
        // as this geometry, so we shouldn't lose any information
        // but we grab the charge and spin from this $molecule block
        else if(strstr(buffer,"OPTIMIZATION CONVERGED") != NULL)
          {
            // Unfortunately this will come in a Z-matrix, which would
            // change our frame of reference -- we'll ignore this geometry
            ifs.getline(buffer,BUFF_SIZE); // *****
            ifs.getline(buffer,BUFF_SIZE); // blank
            ifs.getline(buffer,BUFF_SIZE); // Z-matrix Print:
            ifs.getline(buffer,BUFF_SIZE); // $molecule
            ifs.getline(buffer,BUFF_SIZE); // Charge,Spin
            tokenize(vs, buffer, ", \t\n");
            if (vs.size() == 2)
              {
                charge = atoi(vs[0].c_str());
                spin = atoi(vs[1].c_str());
              }
          } // end (OPTIMIZATION CONVERGED)
      } // end while

    if (mol.NumAtoms() == 0) { // e.g., if we're at the end of a file PR#1737209
      mol.EndModify();
      return false;
    }

    if (!pConv->IsOption("b",OBConversion::INOPTIONS))
      mol.ConnectTheDots();
    if (!pConv->IsOption("s",OBConversion::INOPTIONS) && !pConv->IsOption("b",OBConversion::INOPTIONS))
      mol.PerceiveBondOrders();

    mol.EndModify();
  
    if (hasPartialCharges) {
      mol.SetPartialChargesPerceived();
      // Annotate that partial charges come from Q-Chem Mulliken
      OBPairData *dp = new OBPairData;
      dp->SetAttribute("PartialCharges");
      dp->SetValue("Mulliken");
      dp->SetOrigin(fileformatInput);
      mol.SetData(dp);
    }
    if (hasDipoleMoment) {
      OBVectorData *dmData = new OBVectorData;
      dmData->SetAttribute("Dipole Moment");
      dmData->SetData(dipoleMoment);
      dmData->SetOrigin(fileformatInput);
      mol.SetData(dmData);
    }
    if (hasVibData) {
      OBVibrationData *vd = new OBVibrationData;
      vd->SetData(displacements, frequencies, intensities);
      mol.SetData(vd);
    }
    mol.SetTotalCharge(charge);
    mol.SetTotalSpinMultiplicity(spin);

    mol.SetTitle(title);
    return(true);
  }

  ////////////////////////////////////////////////////////////////

  bool QChemInputFormat::WriteMolecule(OBBase* pOb, OBConversion* pConv)
  {
    OBMol* pmol = dynamic_cast<OBMol*>(pOb);
    if(pmol==NULL)
      return false;

    //Define some references so we can use the old parameter names
    ostream &ofs = *pConv->GetOutStream();
    OBMol &mol = *pmol;

    //    unsigned int i;
    //    OBAtom *atom;

    ofs << "$comment" << endl;
    ofs << mol.GetTitle() << endl;
    ofs << "$end" << endl;

    ofs << endl << "$molecule" << endl;
    ofs << mol.GetTotalCharge() << " " << mol.GetTotalSpinMultiplicity() << endl;

    FOR_ATOMS_OF_MOL(atom, mol)
      {
        ofs << atom->GetAtomicNum() << " "
            << atom->GetX() << " " << atom->GetY() << " " << atom->GetZ() << endl;
      }
    ofs << "$end" << endl;
    ofs << endl << "$rem" << endl;

    const char *keywords = pConv->IsOption("k",OBConversion::OUTOPTIONS);
    const char *keywordFile = pConv->IsOption("f",OBConversion::OUTOPTIONS);
    string defaultKeywords = "";

    if(keywords)
      {
        defaultKeywords = keywords;
      }

    if (keywordFile)
      {
        ifstream kfstream(keywordFile);
        string keyBuffer;
        if (kfstream)
          {
            while (getline(kfstream, keyBuffer))
              ofs << keyBuffer << endl;
          }
      }
    else
      ofs << defaultKeywords << endl;

    ofs << "$end" << endl;

    return(true);
  }

} //namespace OpenBabel

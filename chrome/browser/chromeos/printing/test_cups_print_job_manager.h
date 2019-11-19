// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_TEST_CUPS_PRINT_JOB_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_TEST_CUPS_PRINT_JOB_MANAGER_H_

#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"

class Profile;

namespace chromeos {

class TestCupsPrintJobManager : public CupsPrintJobManager {
 public:
  explicit TestCupsPrintJobManager(Profile* profile);
  ~TestCupsPrintJobManager() override;

  // CupsPrintJobManager:
  void CancelPrintJob(CupsPrintJob* job) override;
  bool SuspendPrintJob(CupsPrintJob* job) override;
  bool ResumePrintJob(CupsPrintJob* job) override;

  void CreatePrintJob(CupsPrintJob* job);
  void StartPrintJob(CupsPrintJob* job);
  void FailPrintJob(CupsPrintJob* job);
  void CompletePrintJob(CupsPrintJob* job);

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCupsPrintJobManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_TEST_CUPS_PRINT_JOB_MANAGER_H_

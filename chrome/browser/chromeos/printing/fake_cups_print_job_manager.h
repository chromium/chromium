// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_FAKE_CUPS_PRINT_JOB_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_FAKE_CUPS_PRINT_JOB_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"

class Profile;

namespace chromeos {

class FakeCupsPrintJobManager : public CupsPrintJobManager {
 public:
  explicit FakeCupsPrintJobManager(Profile* profile);
  ~FakeCupsPrintJobManager() override;

  bool CreatePrintJob(const std::string& printer_name,
                      const std::string& title,
                      int total_page_number);

  void CancelPrintJob(CupsPrintJob* job) override;
  bool SuspendPrintJob(CupsPrintJob* job) override;
  bool ResumePrintJob(CupsPrintJob* job) override;

 private:
  void ChangePrintJobState(CupsPrintJob* job);

  using PrintJobs = std::vector<std::unique_ptr<CupsPrintJob>>;

  PrintJobs print_jobs_;
  static int next_job_id_;
  base::WeakPtrFactory<FakeCupsPrintJobManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeCupsPrintJobManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_FAKE_CUPS_PRINT_JOB_MANAGER_H_

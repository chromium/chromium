// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_FAKE_CUPS_PRINT_JOB_MANAGER_H_
#define CHROME_BROWSER_ASH_PRINTING_FAKE_CUPS_PRINT_JOB_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"

class Profile;

namespace ash {

class FakeCupsPrintJobManager : public chromeos::CupsPrintJobManager {
 public:
  explicit FakeCupsPrintJobManager(Profile* profile);
  FakeCupsPrintJobManager(const FakeCupsPrintJobManager&) = delete;
  FakeCupsPrintJobManager& operator=(const FakeCupsPrintJobManager&) = delete;
  ~FakeCupsPrintJobManager() override;

  bool CreatePrintJob(
      const std::string& printer_id,
      const std::string& title,
      int job_id,
      int total_page_number,
      ::printing::PrintJob::Source source,
      const std::string& source_id,
      const chromeos::printing::proto::PrintSettings& settings) override;

  void CancelPrintJob(chromeos::CupsPrintJob* job) override;
  bool SuspendPrintJob(chromeos::CupsPrintJob* job) override;
  bool ResumePrintJob(chromeos::CupsPrintJob* job) override;

 private:
  void ChangePrintJobState(chromeos::CupsPrintJob* job);

  using PrintJobs = std::vector<std::unique_ptr<chromeos::CupsPrintJob>>;

  PrintJobs print_jobs_;
  base::WeakPtrFactory<FakeCupsPrintJobManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_FAKE_CUPS_PRINT_JOB_MANAGER_H_

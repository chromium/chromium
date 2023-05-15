// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_TEST_CUPS_PRINT_JOB_MANAGER_H_
#define CHROME_BROWSER_ASH_PRINTING_TEST_CUPS_PRINT_JOB_MANAGER_H_

#include "chrome/browser/ash/printing/cups_print_job_manager.h"

class Profile;

namespace ash {

class TestCupsPrintJobManager : public CupsPrintJobManager {
 public:
  explicit TestCupsPrintJobManager(Profile* profile);
  TestCupsPrintJobManager(const TestCupsPrintJobManager&) = delete;
  TestCupsPrintJobManager& operator=(const TestCupsPrintJobManager&) = delete;
  ~TestCupsPrintJobManager() override;

  // CupsPrintJobManager:
  bool CreatePrintJob(const std::string& printer_id,
                      const std::string& title,
                      uint32_t job_id,
                      int total_page_number,
                      ::printing::PrintJob::Source source,
                      const std::string& source_id,
                      const printing::proto::PrintSettings& settings) override;
  void CancelPrintJob(CupsPrintJob* job) override;
  bool SuspendPrintJob(CupsPrintJob* job) override;
  bool ResumePrintJob(CupsPrintJob* job) override;

  void CreatePrintJob(CupsPrintJob* job);
  void StartPrintJob(CupsPrintJob* job);
  void FailPrintJob(CupsPrintJob* job);
  void CompletePrintJob(CupsPrintJob* job);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_TEST_CUPS_PRINT_JOB_MANAGER_H_

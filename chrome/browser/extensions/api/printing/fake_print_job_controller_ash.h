// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PRINTING_FAKE_PRINT_JOB_CONTROLLER_ASH_H_
#define CHROME_BROWSER_EXTENSIONS_API_PRINTING_FAKE_PRINT_JOB_CONTROLLER_ASH_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/printing/cups_print_job_manager.h"
#include "chrome/browser/printing/print_job_controller.h"

namespace ash {
class CupsPrintersManager;
class CupsPrintJob;
class TestCupsPrintJobManager;
}  // namespace ash

namespace extensions {

// Fake print job controller which doesn't send print jobs to actual printing
// pipeline.
// It's used in API integration tests.
class FakePrintJobControllerAsh : public printing::PrintJobController,
                                  public ash::CupsPrintJobManager::Observer {
 public:
  FakePrintJobControllerAsh(ash::TestCupsPrintJobManager* print_job_manager,
                            ash::CupsPrintersManager* printers_manager);
  ~FakePrintJobControllerAsh() override;

  // CupsPrintJobManager::Observer:
  void OnPrintJobDone(base::WeakPtr<ash::CupsPrintJob> job) override;
  void OnPrintJobError(base::WeakPtr<ash::CupsPrintJob> job) override;
  void OnPrintJobCancelled(base::WeakPtr<ash::CupsPrintJob> job) override;

  // PrintJobController:
  void CreatePrintJob(std::unique_ptr<printing::MetafileSkia> pdf,
                      std::unique_ptr<printing::PrintSettings> settings,
                      crosapi::mojom::PrintJob::Source source,
                      const std::string& source_id,
                      PrintJobCreatedCallback callback) override;

 private:
  void CreatePrintJobImpl(scoped_refptr<printing::PrintJob> job,
                          std::unique_ptr<printing::PrintSettings> settings,
                          crosapi::mojom::PrintJob::Source source,
                          const std::string& source_id);

  // Not owned by FakePrintJobControllerAsh.
  const raw_ptr<ash::TestCupsPrintJobManager> print_job_manager_;
  const raw_ptr<ash::CupsPrintersManager> printers_manager_;

  // Stores ongoing print jobs as a mapping from job id to CupsPrintJob.
  base::flat_map<std::string, std::unique_ptr<ash::CupsPrintJob>> jobs_;

  // Current job id.
  int job_id_ = 0;

  base::WeakPtrFactory<FakePrintJobControllerAsh> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PRINTING_FAKE_PRINT_JOB_CONTROLLER_ASH_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/fake_print_job_controller.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/printing/print_job.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Subclass PrintJob to allow construction without supplying a PrintJobManager
// instance.
class PrintJobForTesting : public printing::PrintJob {
 public:
  PrintJobForTesting() = default;

 private:
  ~PrintJobForTesting() override = default;
};

FakePrintJobController::FakePrintJobController() = default;

FakePrintJobController::~FakePrintJobController() = default;

scoped_refptr<printing::PrintJob> FakePrintJobController::StartPrintJob(
    const std::string& extension_id,
    std::unique_ptr<printing::MetafileSkia> metafile,
    std::unique_ptr<printing::PrintSettings> settings) {
  auto job = base::MakeRefCounted<PrintJobForTesting>();
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FakePrintJobController::StartPrinting,
                                weak_ptr_factory_.GetWeakPtr(), job,
                                extension_id, std::move(settings)));
  return job;
}

void FakePrintJobController::StartPrinting(
    scoped_refptr<printing::PrintJob> job,
    const std::string& extension_id,
    std::unique_ptr<printing::PrintSettings> settings) {
  job_id_++;
  job->SetSource(printing::PrintJob::Source::kExtension, extension_id);
  auto document = base::MakeRefCounted<printing::PrintedDocument>(
      std::move(settings), std::u16string(), 0);
  int observer_count = 0;
  for (auto& observer : job->GetObserversForTesting()) {
    if (fail_)
      observer.OnFailed();
    else
      observer.OnDocDone(job_id_, document.get());
    observer_count++;
  }
  EXPECT_EQ(1, observer_count);
}

}  // namespace extensions

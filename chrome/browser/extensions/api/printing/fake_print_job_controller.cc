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

void FakePrintJobController::CreatePrintJob(
    std::unique_ptr<printing::MetafileSkia> pdf,
    std::unique_ptr<printing::PrintSettings> settings,
    uint32_t page_count,
    crosapi::mojom::PrintJob::Source source,
    const std::string& source_id,
    PrintJobCreatedCallback callback) {
  auto job = base::MakeRefCounted<PrintJobForTesting>();
  job->SetSource(source, source_id);
  StartWatchingPrintJob(job, std::move(callback));

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FakePrintJobController::CreatePrintJobImpl,
                     weak_ptr_factory_.GetWeakPtr(), job, std::move(settings)));
}

void FakePrintJobController::CreatePrintJobImpl(
    scoped_refptr<printing::PrintJob> job,
    std::unique_ptr<printing::PrintSettings> settings) {
  job_id_++;
  auto document = base::MakeRefCounted<printing::PrintedDocument>(
      std::move(settings), std::u16string(),
      printing::PrintSettings::NewCookie());
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

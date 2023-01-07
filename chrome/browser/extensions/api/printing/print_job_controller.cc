// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/print_job_controller.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/printer_query.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/global_routing_id.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"

namespace extensions {

namespace {

using PrinterQueryCallback =
    base::OnceCallback<void(std::unique_ptr<printing::PrinterQuery>)>;

// Send initialized PrinterQuery to UI thread.
void OnSettingsSetOnIOThread(std::unique_ptr<printing::PrinterQuery> query,
                             PrinterQueryCallback callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(query)));
}

void CreateQueryOnIOThread(std::unique_ptr<printing::PrintSettings> settings,
                           PrinterQueryCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto query =
      printing::PrinterQuery::Create(content::GlobalRenderFrameHostId());
  auto* query_ptr = query.get();
  query_ptr->SetSettingsFromPOD(
      std::move(settings),
      base::BindOnce(&OnSettingsSetOnIOThread, std::move(query),
                     std::move(callback)));
}

void StartPrinting(scoped_refptr<printing::PrintJob> job,
                   const std::string& extension_id,
                   std::unique_ptr<printing::MetafileSkia> metafile,
                   std::unique_ptr<printing::PrinterQuery> query) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Save in separate variable because |query| is moved.
  std::u16string title = query->settings().title();
  job->Initialize(std::move(query), title, /*page_count=*/1);
  job->SetSource(printing::PrintJob::Source::EXTENSION, extension_id);
  job->document()->SetDocument(std::move(metafile));
  job->StartPrinting();
}

}  // namespace

// This class lives on UI thread.
class PrintJobControllerImpl : public PrintJobController {
 public:
  PrintJobControllerImpl() = default;
  ~PrintJobControllerImpl() override = default;

  // PrintJobController:
  scoped_refptr<printing::PrintJob> StartPrintJob(
      const std::string& extension_id,
      std::unique_ptr<printing::MetafileSkia> metafile,
      std::unique_ptr<printing::PrintSettings> settings) override;
};

scoped_refptr<printing::PrintJob> PrintJobControllerImpl::StartPrintJob(
    const std::string& extension_id,
    std::unique_ptr<printing::MetafileSkia> metafile,
    std::unique_ptr<printing::PrintSettings> settings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto job = base::MakeRefCounted<printing::PrintJob>(
      g_browser_process->print_job_manager());
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CreateQueryOnIOThread, std::move(settings),
                                base::BindOnce(StartPrinting, job, extension_id,
                                               std::move(metafile))));
  return job;
}

// static
std::unique_ptr<PrintJobController> PrintJobController::Create() {
  return std::make_unique<PrintJobControllerImpl>();
}

}  // namespace extensions

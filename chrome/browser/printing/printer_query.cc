// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printer_query.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "printing/buildflags/buildflags.h"
#include "printing/print_settings.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_job_worker_oop.h"
#include "printing/printing_features.h"
#endif

namespace printing {

namespace {

CreatePrintJobWorkerCallback* g_create_print_job_worker_for_testing = nullptr;

std::unique_ptr<PrintJobWorker> CreateWorker(
    content::GlobalRenderFrameHostId rfh_id) {
  if (g_create_print_job_worker_for_testing)
    return g_create_print_job_worker_for_testing->Run(rfh_id);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (features::kEnableOopPrintDriversJobPrint.Get())
    return std::make_unique<PrintJobWorkerOop>(rfh_id);
#endif
  return std::make_unique<PrintJobWorker>(rfh_id);
}

}  // namespace

PrinterQuery::PrinterQuery(content::GlobalRenderFrameHostId rfh_id)
    : cookie_(PrintSettings::NewCookie()), worker_(CreateWorker(rfh_id)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

PrinterQuery::~PrinterQuery() {
  // The job should be finished (or at least canceled) when it is destroyed.
  DCHECK(!is_print_dialog_box_shown_);
  // If this fires, it is that this pending printer context has leaked.
  DCHECK(!worker_);
}

void PrinterQuery::GetSettingsDone(base::OnceClosure callback,
                                   absl::optional<bool> maybe_is_modifiable,
                                   std::unique_ptr<PrintSettings> new_settings,
                                   mojom::ResultCode result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  is_print_dialog_box_shown_ = false;
  last_status_ = result;
  if (result == mojom::ResultCode::kSuccess) {
    settings_ = std::move(new_settings);
    if (maybe_is_modifiable.has_value())
      settings_->set_is_modifiable(maybe_is_modifiable.value());
    cookie_ = PrintSettings::NewCookie();
  } else {
    // Failure.
    cookie_ = 0;
  }

  std::move(callback).Run();
}

void PrinterQuery::PostSettingsDone(base::OnceClosure callback,
                                    absl::optional<bool> maybe_is_modifiable,
                                    std::unique_ptr<PrintSettings> new_settings,
                                    mojom::ResultCode result) {
  // `this` is owned by `callback`, so `base::Unretained()` is safe.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterQuery::GetSettingsDone, base::Unretained(this),
                     std::move(callback), maybe_is_modifiable,
                     std::move(new_settings), result));
}

std::unique_ptr<PrintJobWorker> PrinterQuery::DetachWorker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(worker_);

  return std::move(worker_);
}

const PrintSettings& PrinterQuery::settings() const {
  return *settings_;
}

std::unique_ptr<PrintSettings> PrinterQuery::ExtractSettings() {
  return std::move(settings_);
}

void PrinterQuery::SetSettingsForTest(std::unique_ptr<PrintSettings> settings) {
  settings_ = std::move(settings);
}

int PrinterQuery::cookie() const {
  return cookie_;
}

void PrinterQuery::GetDefaultSettings(base::OnceClosure callback,
                                      bool is_modifiable,
                                      bool want_pdf_settings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  StartWorker();

  // Real work is done in `PrintJobWorker`.
  is_print_dialog_box_shown_ = false;

  if (want_pdf_settings) {
    // `PrintJobWorker::GetPdfSettings()` is always guaranteed to succeed.
    std::unique_ptr<PrintSettings> pdf_settings = worker_->GetPdfSettings();
    DCHECK(pdf_settings);
    PostSettingsDone(std::move(callback), is_modifiable,
                     std::move(pdf_settings), mojom::ResultCode::kSuccess);
    return;
  }

  // `this` is owned by `callback`, so `base::Unretained()` is safe.
  worker_->GetDefaultSettings(
      base::BindOnce(&PrinterQuery::PostSettingsDone, base::Unretained(this),
                     std::move(callback), is_modifiable));
}

void PrinterQuery::GetSettingsFromUser(uint32_t expected_page_count,
                                       bool has_selection,
                                       mojom::MarginType margin_type,
                                       bool is_scripted,
                                       bool is_modifiable,
                                       base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!is_print_dialog_box_shown_ || !is_scripted);

  StartWorker();

  // Real work is done in PrintJobWorker::GetSettingsFromUser().
  is_print_dialog_box_shown_ = true;
  // `this` is owned by `callback`, so `base::Unretained()` is safe.
  worker_->GetSettingsFromUser(
      expected_page_count, has_selection, margin_type, is_scripted,
      base::BindOnce(&PrinterQuery::PostSettingsDone, base::Unretained(this),
                     std::move(callback), is_modifiable));
}

void PrinterQuery::SetSettings(base::Value::Dict new_settings,
                               base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  StartWorker();
  // `this` is owned by `callback`, so `base::Unretained()` is safe.
  worker_->SetSettings(
      std::move(new_settings),
      base::BindOnce(&PrinterQuery::PostSettingsDone, base::Unretained(this),
                     std::move(callback),
                     /*maybe_is_modifiable=*/absl::nullopt));
}

#if BUILDFLAG(IS_CHROMEOS)
void PrinterQuery::SetSettingsFromPOD(
    std::unique_ptr<printing::PrintSettings> new_settings,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  StartWorker();
  // `this` is owned by `callback`, so `base::Unretained()` is safe.
  worker_->SetSettingsFromPOD(
      std::move(new_settings),
      base::BindOnce(&PrinterQuery::PostSettingsDone, base::Unretained(this),
                     std::move(callback),
                     /*maybe_is_modifiable=*/absl::nullopt));
}
#endif

void PrinterQuery::StartWorker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(worker_);

  // Lazily create the worker thread. There is one worker thread per print job.
  if (!worker_->IsRunning())
    worker_->Start();
}

void PrinterQuery::StopWorker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (worker_) {
    // http://crbug.com/66082: We're blocking on the PrinterQuery's worker
    // thread.  It's not clear to me if this may result in blocking the current
    // thread for an unacceptable time.  We should probably fix it.
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_thread_join;
    worker_->Stop();
    worker_.reset();
  }
}

bool PrinterQuery::PostTask(const base::Location& from_here,
                            base::OnceClosure task) {
  return content::GetUIThreadTaskRunner({})->PostTask(from_here,
                                                      std::move(task));
}

// static
void PrinterQuery::SetCreatePrintJobWorkerCallbackForTest(
    CreatePrintJobWorkerCallback* callback) {
  g_create_print_job_worker_for_testing = callback;
}

bool PrinterQuery::is_valid() const {
  return !!worker_;
}

}  // namespace printing

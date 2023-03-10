// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/test_print_view_manager.h"

#include <functional>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/printing/printer_query.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"

namespace printing {

namespace {

void OnDidUpdatePrintSettings(
    std::unique_ptr<PrintSettings>& snooped_settings,
    scoped_refptr<PrintQueriesQueue> queue,
    std::unique_ptr<PrinterQuery> printer_query,
    mojom::PrintManagerHost::UpdatePrintSettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(printer_query);
  auto params = mojom::PrintPagesParams::New();
  params->params = mojom::PrintParams::New();
  if (printer_query->last_status() == mojom::ResultCode::kSuccess) {
    RenderParamsFromPrintSettings(printer_query->settings(),
                                  params->params.get());
    params->params->document_cookie = printer_query->cookie();
    params->pages = printer_query->settings().ranges();
    snooped_settings =
        std::make_unique<PrintSettings>(printer_query->settings());
  }
  bool canceled = printer_query->last_status() == mojom::ResultCode::kCanceled;

  std::move(callback).Run(std::move(params), canceled);

  if (printer_query->cookie() && printer_query->settings().dpi()) {
    queue->QueuePrinterQuery(std::move(printer_query));
  }
}

}  // namespace

TestPrintViewManager::TestPrintViewManager(content::WebContents* web_contents)
    : PrintViewManager(web_contents) {}

TestPrintViewManager::TestPrintViewManager(content::WebContents* web_contents,
                                           OnDidCreatePrintJobCallback callback)
    : PrintViewManager(web_contents),
      on_did_create_print_job_(std::move(callback)) {}

TestPrintViewManager::~TestPrintViewManager() = default;

bool TestPrintViewManager::StartPrinting(content::WebContents* contents) {
  auto* print_view_manager = TestPrintViewManager::FromWebContents(contents);
  if (!print_view_manager) {
    return false;
  }

  content::RenderFrameHost* rfh_to_use = GetFrameToPrint(contents);
  if (!rfh_to_use) {
    return false;
  }

  return print_view_manager->PrintNow(rfh_to_use);
}

void TestPrintViewManager::WaitUntilPreviewIsShownOrCancelled() {
  base::RunLoop run_loop;
  base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
  run_loop.Run();
}

// static
TestPrintViewManager* TestPrintViewManager::CreateForWebContents(
    content::WebContents* web_contents) {
  auto manager = std::make_unique<TestPrintViewManager>(web_contents);
  auto* manager_ptr = manager.get();
  web_contents->SetUserData(PrintViewManager::UserDataKey(),
                            std::move(manager));
  return manager_ptr;
}

bool TestPrintViewManager::PrintNow(content::RenderFrameHost* rfh) {
  print_now_result_ = PrintViewManager::PrintNow(rfh);
  return *print_now_result_;
}

bool TestPrintViewManager::CreateNewPrintJob(
    std::unique_ptr<PrinterQuery> query) {
  if (!PrintViewManager::CreateNewPrintJob(std::move(query))) {
    return false;
  }
  if (on_did_create_print_job_) {
    on_did_create_print_job_.Run(print_job_.get());
  }
  return true;
}

void TestPrintViewManager::PrintPreviewAllowedForTesting() {
  if (run_loop_) {
    run_loop_->Quit();
  }
}

void TestPrintViewManager::UpdatePrintSettings(
    base::Value::Dict job_settings,
    UpdatePrintSettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<PrinterQuery> printer_query =
      queue_->CreatePrinterQuery(content::GlobalRenderFrameHostId());
  auto* printer_query_ptr = printer_query.get();
  printer_query_ptr->SetSettings(
      std::move(job_settings),
      base::BindOnce(&OnDidUpdatePrintSettings, std::ref(snooped_settings_),
                     queue_, std::move(printer_query), std::move(callback)));
}

}  // namespace printing

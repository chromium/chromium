// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/test_print_view_manager.h"

#include <functional>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/printing/printer_query.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"

#if BUILDFLAG(IS_WIN)
#include "printing/metafile.h"
#include "printing/print_job_constants.h"
#endif

namespace printing {

namespace {

void OnDidUpdatePrintSettings(
    mojom::PrintPagesParamsPtr& snooped_params,
    mojom::PrintManagerHost::UpdatePrintSettingsCallback callback,
    mojom::PrintPagesParamsPtr settings) {
  snooped_params = mojom::PrintPagesParams::New();
  auto params = mojom::PrintParams::New();

  // Copy over any relevant fields that we want to snoop.
  if (settings) {
    params->dpi = settings->params->dpi;
    params->page_size = settings->params->page_size;
    params->content_size = settings->params->content_size;
    params->printable_area = settings->params->printable_area;
  }
  snooped_params->params = std::move(params);

  std::move(callback).Run(std::move(settings));
}

class TestPrintJob : public PrintJob {
 public:
  explicit TestPrintJob(PrintJobManager* print_job_manager)
      : PrintJob(print_job_manager) {}

#if BUILDFLAG(IS_WIN)
  void set_simulate_pdf_conversion_error_on_page_index(uint32_t page_index) {
    simulate_pdf_conversion_error_on_page_index_ = page_index;
  }
#endif

 private:
  ~TestPrintJob() override = default;

#if BUILDFLAG(IS_WIN)
  // `PrintJob` overrides:
  void OnPdfPageConverted(uint32_t page_index,
                          float scale_factor,
                          std::unique_ptr<MetafilePlayer> metafile) override {
    if (simulate_pdf_conversion_error_on_page_index_.has_value() &&
        page_index == *simulate_pdf_conversion_error_on_page_index_) {
      // Override the page index to simulate an error.
      page_index = kInvalidPageIndex;
    }
    PrintJob::OnPdfPageConverted(page_index, scale_factor, std::move(metafile));
  }

  std::optional<uint32_t> simulate_pdf_conversion_error_on_page_index_;
#endif  // BUILDFLAG(IS_WIN)
};

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
  quit_closure_ = run_loop.QuitClosure();
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

scoped_refptr<PrintJob> TestPrintViewManager::CreatePrintJob(
    PrintJobManager* print_job_manager) {
  auto print_job = base::MakeRefCounted<TestPrintJob>(print_job_manager);
#if BUILDFLAG(IS_WIN)
  if (simulate_pdf_conversion_error_on_page_index_.has_value()) {
    print_job->set_simulate_pdf_conversion_error_on_page_index(
        *simulate_pdf_conversion_error_on_page_index_);
  }
#endif
  if (on_did_create_print_job_) {
    on_did_create_print_job_.Run(print_job.get());
  }
  return print_job;
}

void TestPrintViewManager::PrintPreviewRejectedForTesting() {
  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

void TestPrintViewManager::PrintPreviewAllowedForTesting() {
  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

void TestPrintViewManager::UpdatePrintSettings(
    base::Value::Dict job_settings,
    UpdatePrintSettingsCallback callback) {
  PrintViewManagerBase::UpdatePrintSettings(
      std::move(job_settings),
      base::BindOnce(&OnDidUpdatePrintSettings, std::ref(snooped_params_),
                     std::move(callback)));
}

}  // namespace printing

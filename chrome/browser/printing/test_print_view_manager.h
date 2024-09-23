// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_TEST_PRINT_VIEW_MANAGER_H_
#define CHROME_BROWSER_PRINTING_TEST_PRINT_VIEW_MANAGER_H_

#include <optional>

#include "base/functional/callback.h"
#include "chrome/browser/printing/print_view_manager.h"

namespace content {
class WebContents;
}  // namespace content

namespace printing {

class PrintJob;

using OnDidCreatePrintJobCallback =
    base::RepeatingCallback<void(PrintJob* print_job)>;

class TestPrintViewManager : public PrintViewManager {
 public:
  explicit TestPrintViewManager(content::WebContents* web_contents);
  TestPrintViewManager(content::WebContents* web_contents,
                       OnDidCreatePrintJobCallback callback);
  TestPrintViewManager(const TestPrintViewManager&) = delete;
  TestPrintViewManager& operator=(const TestPrintViewManager&) = delete;
  ~TestPrintViewManager() override;

#if BUILDFLAG(IS_WIN)
  void set_simulate_pdf_conversion_error_on_page_index(uint32_t page_index) {
    simulate_pdf_conversion_error_on_page_index_ = page_index;
  }
#endif

  bool StartPrinting(content::WebContents* contents);

  void WaitUntilPreviewIsShownOrCancelled();

  const mojom::PrintPagesParamsPtr& snooped_params() const {
    return snooped_params_;
  }

  const std::optional<bool>& print_now_result() const {
    return print_now_result_;
  }

  static TestPrintViewManager* CreateForWebContents(
      content::WebContents* web_contents);

  // `PrintViewManagerBase` overrides.
  bool PrintNow(content::RenderFrameHost* rfh) override;

 protected:
  // `PrintViewManager` overrides
  void PrintPreviewRejectedForTesting() override;
  void PrintPreviewAllowedForTesting() override;

 private:
  // `PrintViewManagerBase` overrides.
  scoped_refptr<PrintJob> CreatePrintJob(
      PrintJobManager* print_job_manager) override;

  // printing::mojom::PrintManagerHost:
  void UpdatePrintSettings(base::Value::Dict job_settings,
                           UpdatePrintSettingsCallback callback) override;

  base::OnceClosure quit_closure_;
  mojom::PrintPagesParamsPtr snooped_params_;
  std::optional<bool> print_now_result_;
#if BUILDFLAG(IS_WIN)
  std::optional<uint32_t> simulate_pdf_conversion_error_on_page_index_;
#endif
  OnDidCreatePrintJobCallback on_did_create_print_job_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_TEST_PRINT_VIEW_MANAGER_H_

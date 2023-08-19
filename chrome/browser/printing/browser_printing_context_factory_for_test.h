// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_BROWSER_PRINTING_CONTEXT_FACTORY_FOR_TEST_H_
#define CHROME_BROWSER_PRINTING_BROWSER_PRINTING_CONTEXT_FACTORY_FOR_TEST_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"
#include "printing/printing_context.h"
#include "printing/printing_context_factory_for_test.h"
#include "printing/test_printing_context.h"

namespace printing {

class BrowserPrintingContextFactoryForTest
    : public PrintingContextFactoryForTest {
 public:
  BrowserPrintingContextFactoryForTest();
  ~BrowserPrintingContextFactoryForTest();

  std::unique_ptr<PrintingContext> CreatePrintingContext(
      PrintingContext::Delegate* delegate,
      bool skip_system_calls) override;

  void SetPrinterNameForSubsequentContexts(const std::string& printer_name);
  void SetCancelErrorOnNewDocument(bool cause_errors);
  void SetFailedErrorOnNewDocument(bool cause_errors);
  void SetAccessDeniedErrorOnNewDocument(bool cause_errors);
#if BUILDFLAG(IS_WIN)
  void SetAccessDeniedErrorOnRenderPage(bool cause_errors);
  void SetFailedErrorForRenderPage(uint32_t page_number);
#endif
  void SetAccessDeniedErrorOnRenderDocument(bool cause_errors);
  void SetAccessDeniedErrorOnDocumentDone(bool cause_errors);
  void SetFailErrorOnUseDefaultSettings();
#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  void SetCancelErrorOnAskUserForSettings();
#endif
  void SetOnNewDocumentCallback(
      TestPrintingContext::OnNewDocumentCallback callback);

 private:
  std::string printer_name_;
  bool cancels_in_new_document_ = false;
  bool failed_error_for_new_document_ = false;
  bool access_denied_errors_for_new_document_ = false;
#if BUILDFLAG(IS_WIN)
  bool access_denied_errors_for_render_page_ = false;
  uint32_t failed_error_for_render_page_number_ = 0;
#endif
  bool access_denied_errors_for_render_document_ = false;
  bool access_denied_errors_for_document_done_ = false;
  bool fail_on_use_default_settings_ = false;
#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  bool cancel_on_ask_user_for_settings_ = false;
#endif
  TestPrintingContext::OnNewDocumentCallback on_new_document_callback_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_BROWSER_PRINTING_CONTEXT_FACTORY_FOR_TEST_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/browser_printing_context_factory_for_test.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "printing/buildflags/buildflags.h"
#include "printing/test_printing_context.h"

namespace printing {

BrowserPrintingContextFactoryForTest::BrowserPrintingContextFactoryForTest() =
    default;

BrowserPrintingContextFactoryForTest::~BrowserPrintingContextFactoryForTest() =
    default;

std::unique_ptr<PrintingContext>
BrowserPrintingContextFactoryForTest::CreatePrintingContext(
    PrintingContext::Delegate* delegate,
    PrintingContext::ProcessBehavior process_behavior) {
  auto context =
      MakeDefaultTestPrintingContext(delegate, process_behavior, printer_name_);

  if (new_document_job_id_.has_value()) {
    context->SetNewDocumentJobId(new_document_job_id_.value());
  }

  if (failed_error_for_update_printer_settings_) {
    context->SetUpdatePrinterSettingsFails();
  }
  if (cancels_in_new_document_) {
    context->SetNewDocumentCancels();
  }
  if (failed_error_for_new_document_) {
    context->SetNewDocumentFails();
  }
  if (access_denied_errors_for_new_document_) {
    context->SetNewDocumentBlockedByPermissions();
  }
#if BUILDFLAG(IS_WIN)
  if (access_denied_errors_for_render_page_) {
    context->SetOnRenderPageBlockedByPermissions();
  }
  if (failed_error_for_render_page_number_) {
    context->SetOnRenderPageFailsForPage(failed_error_for_render_page_number_);
  }
#endif
  if (access_denied_errors_for_render_document_) {
    context->SetOnRenderDocumentBlockedByPermissions();
  }
  if (access_denied_errors_for_document_done_) {
    context->SetDocumentDoneBlockedByPermissions();
  }

  if (fail_on_use_default_settings_) {
    context->SetUseDefaultSettingsFails();
  }
#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  if (cancel_on_ask_user_for_settings_) {
    context->SetAskUserForSettingsCanceled();
  }

  if (fail_on_ask_user_for_settings_) {
    context->SetAskUserForSettingsFails();
  }
#endif

  context->SetUserSettings(*test::MakeUserModifiedPrintSettings(
      printer_name_, base::OptionalToPtr(page_ranges_)));

  context->SetOnNewDocumentCallback(on_new_document_callback_);

  return context;
}

void BrowserPrintingContextFactoryForTest::SetPrinterNameForSubsequentContexts(
    const std::string& printer_name) {
  printer_name_ = printer_name;
}

#if BUILDFLAG(IS_WIN)
void BrowserPrintingContextFactoryForTest::
    SetPrinterLanguageTypeForSubsequentContexts(
        mojom::PrinterLanguageType printer_language_type) {
  printer_language_type_ = printer_language_type;
}
#endif

void BrowserPrintingContextFactoryForTest::
    SetUserSettingsPageRangesForSubsequentContext(
        const PageRanges& page_ranges) {
  page_ranges_ = page_ranges;
}

void BrowserPrintingContextFactoryForTest::
    SetFailedErrorOnUpdatePrinterSettings() {
  failed_error_for_update_printer_settings_ = true;
}

void BrowserPrintingContextFactoryForTest::SetCancelErrorOnNewDocument(
    bool cause_errors) {
  cancels_in_new_document_ = cause_errors;
}

void BrowserPrintingContextFactoryForTest::SetFailedErrorOnNewDocument(
    bool cause_errors) {
  failed_error_for_new_document_ = cause_errors;
}

void BrowserPrintingContextFactoryForTest::SetJobIdOnNewDocument(int job_id) {
  new_document_job_id_ = job_id;
}

void BrowserPrintingContextFactoryForTest::SetAccessDeniedErrorOnNewDocument(
    bool cause_errors) {
  access_denied_errors_for_new_document_ = cause_errors;
}

#if BUILDFLAG(IS_WIN)
void BrowserPrintingContextFactoryForTest::SetAccessDeniedErrorOnRenderPage(
    bool cause_errors) {
  access_denied_errors_for_render_page_ = cause_errors;
}

void BrowserPrintingContextFactoryForTest::SetFailedErrorForRenderPage(
    uint32_t page_number) {
  failed_error_for_render_page_number_ = page_number;
}
#endif

void BrowserPrintingContextFactoryForTest::SetAccessDeniedErrorOnRenderDocument(
    bool cause_errors) {
  access_denied_errors_for_render_document_ = cause_errors;
}

void BrowserPrintingContextFactoryForTest::SetAccessDeniedErrorOnDocumentDone(
    bool cause_errors) {
  access_denied_errors_for_document_done_ = cause_errors;
}

void BrowserPrintingContextFactoryForTest::SetFailErrorOnUseDefaultSettings() {
  fail_on_use_default_settings_ = true;
}

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
void BrowserPrintingContextFactoryForTest::
    SetCancelErrorOnAskUserForSettings() {
  cancel_on_ask_user_for_settings_ = true;
}

void BrowserPrintingContextFactoryForTest::SetFailErrorOnAskUserForSettings() {
  fail_on_ask_user_for_settings_ = true;
}
#endif

void BrowserPrintingContextFactoryForTest::SetOnNewDocumentCallback(
    TestPrintingContext::OnNewDocumentCallback callback) {
  on_new_document_callback_ = std::move(callback);
}

std::unique_ptr<TestPrintingContext>
BrowserPrintingContextFactoryForTest::MakeDefaultTestPrintingContext(
    PrintingContext::Delegate* delegate,
    PrintingContext::ProcessBehavior process_behavior,
    const std::string& printer_name) {
  auto context =
      std::make_unique<TestPrintingContext>(delegate, process_behavior);

  std::unique_ptr<PrintSettings> settings =
      test::MakeDefaultPrintSettings(printer_name);

#if BUILDFLAG(IS_WIN)
  if (printer_language_type_.has_value()) {
    settings->set_printer_language_type(printer_language_type_.value());
  }
#endif

  context->SetDeviceSettings(printer_name, std::move(settings));
  return context;
}

}  // namespace printing

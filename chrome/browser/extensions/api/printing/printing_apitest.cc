// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#include "chrome/browser/ash/printing/test_cups_print_job_manager.h"
#include "chrome/browser/extensions/api/printing/fake_print_job_controller_ash.h"
#include "chrome/browser/extensions/api/printing/print_job_submitter.h"
#include "chrome/browser/extensions/api/printing/printing_api_handler.h"
#include "chrome/browser/extensions/api/printing/printing_test_utils.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/test_extension_dir.h"
#include "printing/backend/print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {
constexpr char kId[] = "id";
}  // namespace

class PrintingApiTest : public ExtensionApiTest,
                        public testing::WithParamInterface<ExtensionType> {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    helper_->Init(browser()->profile());
  }

  void TearDownOnMainThread() override {
    helper_.reset();
    ExtensionApiTest::TearDownOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    helper_ = std::make_unique<PrintingTestHelper>();
  }

 protected:
  void RunTest(const char* html_test_page) {
    auto dir = CreatePrintingExtension(GetExtensionType());
    auto run_options = GetExtensionType() == ExtensionType::kChromeApp
                           ? RunOptions{.custom_arg = html_test_page,
                                        .launch_as_platform_app = true}
                           : RunOptions({.extension_url = html_test_page});
    ASSERT_TRUE(RunExtensionTest(dir->UnpackedPath(), run_options, {}));
  }

  PrintingTestHelper* helper() { return helper_.get(); }

 private:
  ExtensionType GetExtensionType() const { return GetParam(); }

  std::unique_ptr<PrintingTestHelper> helper_;
};

using PrintingPromiseApiTest = PrintingApiTest;

IN_PROC_BROWSER_TEST_P(PrintingApiTest, GetPrinters) {
  chromeos::Printer printer = chromeos::Printer(kId);
  printer.set_display_name("name");
  helper()->GetPrintersManager()->AddPrinter(printer,
                                             chromeos::PrinterClass::kSaved);

  RunTest("get_printers.html");
}

IN_PROC_BROWSER_TEST_P(PrintingApiTest, GetPrinterInfo) {
  helper()->AddAvailablePrinter(
      kId, std::make_unique<printing::PrinterSemanticCapsAndDefaults>());

  RunTest("get_printer_info.html");
}

// Verifies that:
// a) PrintingHooksDelegate substitutes corresponding Blob UUID and DCHECK
// doesn't fail.
// b) Whole API arguments handling pipeline works correctly.
// We use fake version of PrintJobController because we don't have a mock
// version of PrintingContext which is required to handle sending print job to
// the printer.
IN_PROC_BROWSER_TEST_P(PrintingApiTest, SubmitJob) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  helper()->AddAvailablePrinter(kId, ConstructPrinterCapabilities());
  PrintingAPIHandler* handler = PrintingAPIHandler::Get(browser()->profile());
  handler->SetPrintJobControllerForTesting(
      std::make_unique<FakePrintJobControllerAsh>(
          helper()->GetPrintJobManager(), helper()->GetPrintersManager()));
  base::AutoReset<bool> skip_confirmation_dialog_reset(
      PrintJobSubmitter::SkipConfirmationDialogForTesting());

  RunTest("submit_job.html");
}

// As above, but tests using promise based API calls.
IN_PROC_BROWSER_TEST_P(PrintingPromiseApiTest, SubmitJob) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  helper()->AddAvailablePrinter(kId, ConstructPrinterCapabilities());
  PrintingAPIHandler* handler = PrintingAPIHandler::Get(browser()->profile());
  handler->SetPrintJobControllerForTesting(
      std::make_unique<FakePrintJobControllerAsh>(
          helper()->GetPrintJobManager(), helper()->GetPrintersManager()));
  base::AutoReset<bool> skip_confirmation_dialog_reset(
      PrintJobSubmitter::SkipConfirmationDialogForTesting());

  RunTest("submit_job_promise.html");
}

// Verifies that:
// a) Cancel job request works smoothly.
// b) OnJobStatusChanged() events are dispatched correctly.
IN_PROC_BROWSER_TEST_P(PrintingApiTest, CancelJob) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  helper()->AddAvailablePrinter(kId, ConstructPrinterCapabilities());
  PrintingAPIHandler* handler = PrintingAPIHandler::Get(browser()->profile());
  handler->SetPrintJobControllerForTesting(
      std::make_unique<FakePrintJobControllerAsh>(
          helper()->GetPrintJobManager(), helper()->GetPrintersManager()));
  base::AutoReset<bool> skip_confirmation_dialog_reset(
      PrintJobSubmitter::SkipConfirmationDialogForTesting());

  RunTest("cancel_job.html");
}

INSTANTIATE_TEST_SUITE_P(/**/,
                         PrintingApiTest,
                         testing::Values(ExtensionType::kChromeApp,
                                         ExtensionType::kExtensionMV2,
                                         ExtensionType::kExtensionMV3));

// We only run the promise based tests for MV3 extensions as promise based API
// calls are only exposed to MV3.
INSTANTIATE_TEST_SUITE_P(/**/,
                         PrintingPromiseApiTest,
                         testing::Values(ExtensionType::kExtensionMV3));

}  // namespace extensions

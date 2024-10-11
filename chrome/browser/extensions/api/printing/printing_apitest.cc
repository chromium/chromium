// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/extensions/api/printing/print_job_submitter.h"
#include "chrome/browser/extensions/api/printing/printing_test_utils.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/printing/local_printer_utils_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kId[] = "id";
constexpr char kName[] = "name";

}  // namespace

class PrintingApiTestBase : public ExtensionApiTest,
                            public testing::WithParamInterface<ExtensionType> {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    PrintJobSubmitter::SkipConfirmationDialogForTesting();
  }

 protected:
  ExtensionType GetExtensionType() const { return GetParam(); }

  void RunTest(const char* html_test_page) {
    auto dir = CreatePrintingExtension(GetExtensionType());
    auto run_options = GetExtensionType() == ExtensionType::kChromeApp
                           ? RunOptions{.custom_arg = html_test_page,
                                        .launch_as_platform_app = true}
                           : RunOptions({.extension_url = html_test_page});
    ASSERT_TRUE(RunExtensionTest(dir->UnpackedPath(), run_options, {}));
  }
};

class PrintingApiTest : public PrintingApiTestBase {
 public:
  void PreRunTestOnMainThread() override {
    PrintingApiTestBase::PreRunTestOnMainThread();
    helper_->Init(browser()->profile());
  }

  void TearDownOnMainThread() override {
    helper_.reset();
    PrintingApiTestBase::TearDownOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    PrintingApiTestBase::SetUpInProcessBrowserTestFixture();
    helper_ = std::make_unique<PrintingTestHelper>();
  }

 protected:
  void AddPrinterWithSemanticCaps(
      const std::string& printer_id,
      const std::string& printer_display_name,
      std::unique_ptr<printing::PrinterSemanticCapsAndDefaults> caps) {
    helper_->AddAvailablePrinter(printer_id, printer_display_name,
                                 std::move(caps));
  }

 private:
  std::unique_ptr<PrintingTestHelper> helper_;
};

using PrintingPromiseApiTest = PrintingApiTest;

IN_PROC_BROWSER_TEST_P(PrintingApiTest, GetPrinters) {
  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());

  RunTest("get_printers.html");
}

IN_PROC_BROWSER_TEST_P(PrintingApiTest, GetPrinterInfo) {
  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());

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

  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());

  RunTest("submit_job.html");
}

// As above, but tests using promise based API calls.
IN_PROC_BROWSER_TEST_P(PrintingPromiseApiTest, SubmitJob) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());

  RunTest("submit_job_promise.html");
}

// Verifies that:
// a) Cancel job request works smoothly.
// b) OnJobStatusChanged() events are dispatched correctly.
IN_PROC_BROWSER_TEST_P(PrintingApiTest, CancelJob) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());

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

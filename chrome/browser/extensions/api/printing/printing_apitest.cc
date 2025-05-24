// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "chrome/browser/extensions/api/printing/print_job_submitter.h"
#include "chrome/browser/extensions/api/printing/printing_test_utils.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/printing/local_printer_utils_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/test_extension_dir.h"
#include "printing/printing_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kId[] = "id";
constexpr char kName[] = "name";

}  // namespace

// TODO(crbug.com/308709702): Remove the bool param from this as soon as
// the `kPrintingMarginsAndScale` feature is enabled by default. At the moment,
// this is used to run the same test with and without the feature enabled.
class PrintingApiTestBase
    : public ExtensionApiTest,
      public testing::WithParamInterface<std::tuple<bool, ExtensionType>> {
 public:
  void SetUp() override {
    if (GetEnableMarginAndScale()) {
      feature_list_.InitAndEnableFeature(
          printing::features::kApiPrintingMarginsAndScale);
    } else {
      feature_list_.InitAndDisableFeature(
          printing::features::kApiPrintingMarginsAndScale);
    }
    ExtensionApiTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    PrintJobSubmitter::SkipConfirmationDialogForTesting();
  }

 protected:
  bool GetEnableMarginAndScale() const { return std::get<0>(GetParam()); }
  ExtensionType GetExtensionType() const { return std::get<1>(GetParam()); }

  void RunTest(const char* html_test_page, bool expect_success = true) {
    auto dir = CreatePrintingExtension(GetExtensionType());
    auto run_options = GetExtensionType() == ExtensionType::kChromeApp
                           ? RunOptions{.custom_arg = html_test_page,
                                        .launch_as_platform_app = true}
                           : RunOptions({.extension_url = html_test_page});
    ASSERT_EQ(RunExtensionTest(dir->UnpackedPath(), run_options, {}),
              expect_success);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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

  // Expect failure/success depending on whether the feature is enabled or not.
  // TODO(crbug.com/308709702): Remove this and merge two files once the feature
  // is enabled by default.
  const bool expect_success = GetEnableMarginAndScale();
  RunTest("get_printer_info_margin_and_scale.html", expect_success);
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

IN_PROC_BROWSER_TEST_P(PrintingApiTest, SubmitJobWithMarginsAndScale) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());

  RunTest("submit_job_margins_and_scale.html");
}

IN_PROC_BROWSER_TEST_P(PrintingApiTest, SubmitJobWithUnsupportedMargins) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // If the feature is disabled, the test must succeed regardless of the margins
  // used.
  // TODO(crbug.com/308709702): Remove this and expect the test to always fail
  // once the feature is enabled by default as provided margins in this test
  // are not supported by the setup printer.
  const bool expect_success = !GetEnableMarginAndScale();

  auto caps = ConstructPrinterCapabilities();
  std::vector<printing::PrinterSemanticCapsAndDefaults::Paper> papers;
  // Override papers with custom margins.
  for (const auto& paper : caps->papers) {
    papers.emplace_back(paper.display_name(), paper.vendor_id(),
                        paper.size_um(), paper.printable_area_um(),
                        paper.max_height_um(), paper.has_borderless_variant(),
                        printing::PaperMargins(2340, 1234, 1234, 1234));
  }
  caps->papers = std::move(papers);
  AddPrinterWithSemanticCaps(kId, kName, std::move(caps));

  RunTest("submit_job_margins_and_scale.html", expect_success);
}

IN_PROC_BROWSER_TEST_P(PrintingApiTest, SubmitJobWithUnsupportedScale) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // If the feature is disabled, the test must succeed regardless of the scale
  // used.
  // TODO(crbug.com/308709702): Remove this and expect the test to always fail
  // once the feature is enabled by default as provided scale in this test
  // is not supported by the setup printer.
  const bool expect_success = !GetEnableMarginAndScale();

  auto caps = ConstructPrinterCapabilities();
  // Override with custom scaling type different from defined in the js/html
  // file of the test.
  caps->print_scaling_types = {printing::mojom::PrintScalingType::kFill};
  caps->print_scaling_type_default = printing::mojom::PrintScalingType::kFill;
  AddPrinterWithSemanticCaps(kId, kName, std::move(caps));

  RunTest("submit_job_margins_and_scale.html", expect_success);
}

// As above, but tests using promise based API calls.
IN_PROC_BROWSER_TEST_P(PrintingPromiseApiTest, SubmitJob) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());

  RunTest("submit_job_promise.html");
}

IN_PROC_BROWSER_TEST_P(PrintingPromiseApiTest, SubmitJobWithMarginsAndScale) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());

  RunTest("submit_job_promise_margins_and_scale.html");
}

// Verifies that:
// a) Cancel job request works smoothly.
// b) OnJobStatusChanged() events are dispatched correctly.
IN_PROC_BROWSER_TEST_P(PrintingApiTest, CancelJob) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());

  RunTest("cancel_job.html");
}

IN_PROC_BROWSER_TEST_P(PrintingApiTest, GetJobStatus) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());

  RunTest("get_print_job_status.html");
}

INSTANTIATE_TEST_SUITE_P(
    /**/,
    PrintingApiTest,
    testing::Combine(testing::Bool(),
                     testing::Values(ExtensionType::kChromeApp,
                                     ExtensionType::kExtensionMV2,
                                     ExtensionType::kExtensionMV3)));

// We only run the promise based tests for MV3 extensions as promise based API
// calls are only exposed to MV3.
INSTANTIATE_TEST_SUITE_P(
    /**/,
    PrintingPromiseApiTest,
    testing::Combine(testing::Bool(),
                     testing::Values(ExtensionType::kExtensionMV3)));

}  // namespace extensions

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#include "chrome/browser/ash/printing/test_cups_print_job_manager.h"
#include "chrome/browser/extensions/api/printing/fake_print_job_controller_ash.h"
#include "chrome/browser/extensions/api/printing/print_job_submitter.h"
#include "chrome/browser/extensions/api/printing/printing_api.h"
#include "chrome/browser/extensions/api/printing/printing_api_handler.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/constants.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kId[] = "id";

constexpr int kHorizontalDpi = 300;
constexpr int kVerticalDpi = 400;
constexpr int kMediaSizeWidth = 210000;
constexpr int kMediaSizeHeight = 297000;
constexpr char kMediaSizeVendorId[] = "iso_a4_210x297mm";

std::unique_ptr<KeyedService> BuildTestCupsPrintJobManager(
    content::BrowserContext* context) {
  return std::make_unique<ash::TestCupsPrintJobManager>(
      Profile::FromBrowserContext(context));
}

std::unique_ptr<KeyedService> BuildFakeCupsPrintersManager(
    content::BrowserContext* context) {
  return std::make_unique<ash::FakeCupsPrintersManager>();
}

std::unique_ptr<printing::PrinterSemanticCapsAndDefaults>
ConstructPrinterCapabilities() {
  auto capabilities =
      std::make_unique<printing::PrinterSemanticCapsAndDefaults>();
  capabilities->color_model = printing::mojom::ColorModel::kColor;
  capabilities->duplex_modes.push_back(printing::mojom::DuplexMode::kSimplex);
  capabilities->copies_max = 2;
  capabilities->dpis.emplace_back(kHorizontalDpi, kVerticalDpi);
  printing::PrinterSemanticCapsAndDefaults::Paper paper(
      /*display_name=*/"", kMediaSizeVendorId,
      {kMediaSizeWidth, kMediaSizeHeight});
  capabilities->papers.push_back(std::move(paper));
  capabilities->collate_capable = true;
  return capabilities;
}

}  // namespace

class PrintingApiTest : public ExtensionApiTest,
                        public testing::WithParamInterface<bool> {
 public:
  PrintingApiTest() = default;
  ~PrintingApiTest() override = default;

  PrintingApiTest(const PrintingApiTest&) = delete;
  PrintingApiTest& operator=(const PrintingApiTest&) = delete;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &PrintingApiTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
    test_print_backend_ = base::MakeRefCounted<printing::TestPrintBackend>();
    printing::PrintBackend::SetPrintBackendForTesting(
        test_print_backend_.get());
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  ash::TestCupsPrintJobManager* GetPrintJobManager() {
    return static_cast<ash::TestCupsPrintJobManager*>(
        ash::CupsPrintJobManagerFactory::GetForBrowserContext(
            browser()->profile()));
  }

  ash::FakeCupsPrintersManager* GetPrintersManager() {
    return static_cast<ash::FakeCupsPrintersManager*>(
        ash::CupsPrintersManagerFactory::GetForBrowserContext(
            browser()->profile()));
  }

  void AddAvailablePrinter(
      const std::string& printer_id,
      std::unique_ptr<printing::PrinterSemanticCapsAndDefaults> capabilities) {
    GetPrintersManager()->AddPrinter(chromeos::Printer(printer_id),
                                     chromeos::PrinterClass::kEnterprise);
    chromeos::CupsPrinterStatus status(printer_id);
    status.AddStatusReason(
        chromeos::CupsPrinterStatus::CupsPrinterStatusReason::Reason::
            kPrinterUnreachable,
        chromeos::CupsPrinterStatus::CupsPrinterStatusReason::Severity::kError);
    GetPrintersManager()->SetPrinterStatus(status);
    test_print_backend_->AddValidPrinter(printer_id, std::move(capabilities),
                                         nullptr);
  }

  void RunTest(const char* html_test_page) {
    TestExtensionDir dir;

    {
      // Prepare test files.
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::CopyDirectory(test_data_dir_.AppendASCII("printing"),
                          dir.UnpackedPath(), /*recursive=*/false);
      base::CopyFile(
          test_data_dir_.AppendASCII("printing")
              .AppendASCII(IsChromeApp() ? "manifest_chrome_app.json"
                                         : "manifest_extension.json"),
          dir.UnpackedPath().AppendASCII(extensions::kManifestFilename));
    }

    auto run_options = IsChromeApp()
                           ? RunOptions{.custom_arg = html_test_page,
                                        .launch_as_platform_app = true}
                           : RunOptions({.extension_url = html_test_page});
    ASSERT_TRUE(RunExtensionTest(dir.UnpackedPath(), run_options, {}));
  }

 private:
  bool IsChromeApp() const { return GetParam(); }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    ash::CupsPrintJobManagerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildTestCupsPrintJobManager));
    ash::CupsPrintersManagerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildFakeCupsPrintersManager));
  }

  base::CallbackListSubscription create_services_subscription_;

  scoped_refptr<printing::TestPrintBackend> test_print_backend_;
};

IN_PROC_BROWSER_TEST_P(PrintingApiTest, GetPrinters) {
  chromeos::Printer printer = chromeos::Printer(kId);
  printer.set_display_name("name");
  GetPrintersManager()->AddPrinter(printer, chromeos::PrinterClass::kSaved);

  RunTest("get_printers.html");
}

IN_PROC_BROWSER_TEST_P(PrintingApiTest, GetPrinterInfo) {
  AddAvailablePrinter(
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

  AddAvailablePrinter(kId, ConstructPrinterCapabilities());
  PrintingAPIHandler* handler = PrintingAPIHandler::Get(browser()->profile());
  handler->SetPrintJobControllerForTesting(
      std::make_unique<FakePrintJobControllerAsh>(GetPrintJobManager(),
                                                  GetPrintersManager()));
  base::AutoReset<bool> skip_confirmation_dialog_reset(
      PrintJobSubmitter::SkipConfirmationDialogForTesting());

  RunTest("submit_job.html");
}

// Verifies that:
// a) Cancel job request works smoothly.
// b) OnJobStatusChanged() events are dispatched correctly.
IN_PROC_BROWSER_TEST_P(PrintingApiTest, CancelJob) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  AddAvailablePrinter(kId, ConstructPrinterCapabilities());
  PrintingAPIHandler* handler = PrintingAPIHandler::Get(browser()->profile());
  handler->SetPrintJobControllerForTesting(
      std::make_unique<FakePrintJobControllerAsh>(GetPrintJobManager(),
                                                  GetPrintersManager()));
  base::AutoReset<bool> skip_confirmation_dialog_reset(
      PrintJobSubmitter::SkipConfirmationDialogForTesting());

  RunTest("cancel_job.html");
}

// |true| for Chrome App, |false| for Extension.
INSTANTIATE_TEST_SUITE_P(/**/, PrintingApiTest, testing::Bool());

}  // namespace extensions

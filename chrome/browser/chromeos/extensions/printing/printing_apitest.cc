// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/chromeos/extensions/printing/fake_print_job_controller.h"
#include "chrome/browser/chromeos/extensions/printing/printing_api.h"
#include "chrome/browser/chromeos/extensions/printing/printing_api_handler.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/test_cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/test_cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/test_printer_configurer.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"
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
  return std::make_unique<chromeos::TestCupsPrintJobManager>(
      Profile::FromBrowserContext(context));
}

std::unique_ptr<KeyedService> BuildTestCupsPrintersManager(
    content::BrowserContext* context) {
  return std::make_unique<chromeos::TestCupsPrintersManager>();
}

std::unique_ptr<printing::PrinterSemanticCapsAndDefaults>
ConstructPrinterCapabilities() {
  auto capabilities =
      std::make_unique<printing::PrinterSemanticCapsAndDefaults>();
  capabilities->color_model = printing::mojom::ColorModel::kColor;
  capabilities->duplex_modes.push_back(printing::mojom::DuplexMode::kSimplex);
  capabilities->copies_max = 2;
  capabilities->dpis.push_back(gfx::Size(kHorizontalDpi, kVerticalDpi));
  printing::PrinterSemanticCapsAndDefaults::Paper paper;
  paper.vendor_id = kMediaSizeVendorId;
  paper.size_um = gfx::Size(kMediaSizeWidth, kMediaSizeHeight);
  capabilities->papers.push_back(paper);
  capabilities->collate_capable = true;
  return capabilities;
}

}  // namespace

class PrintingApiTest : public ExtensionApiTest {
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
    chromeos::PrinterConfigurer::SetPrinterConfigurerForTesting(
        std::make_unique<chromeos::TestPrinterConfigurer>());
    test_print_backend_ = base::MakeRefCounted<printing::TestPrintBackend>();
    printing::PrintBackend::SetPrintBackendForTesting(
        test_print_backend_.get());
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  chromeos::TestCupsPrintJobManager* GetPrintJobManager() {
    return static_cast<chromeos::TestCupsPrintJobManager*>(
        chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(
            browser()->profile()));
  }

  chromeos::TestCupsPrintersManager* GetPrintersManager() {
    return static_cast<chromeos::TestCupsPrintersManager*>(
        chromeos::CupsPrintersManagerFactory::GetForBrowserContext(
            browser()->profile()));
  }

  void AddAvailablePrinter(
      const std::string& printer_id,
      std::unique_ptr<printing::PrinterSemanticCapsAndDefaults> capabilities) {
    chromeos::Printer printer = chromeos::Printer(printer_id);
    GetPrintersManager()->AddPrinter(printer,
                                     chromeos::PrinterClass::kEnterprise);
    test_print_backend_->AddValidPrinter(printer_id, std::move(capabilities));
  }

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    chromeos::CupsPrintJobManagerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildTestCupsPrintJobManager));
    chromeos::CupsPrintersManagerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildTestCupsPrintersManager));
  }

  std::unique_ptr<
      BrowserContextDependencyManager::CreateServicesCallbackList::Subscription>
      create_services_subscription_;

  scoped_refptr<printing::TestPrintBackend> test_print_backend_;
};

IN_PROC_BROWSER_TEST_F(PrintingApiTest, GetPrinters) {
  constexpr char kName[] = "name";

  chromeos::Printer printer = chromeos::Printer(kId);
  printer.set_display_name(kName);
  GetPrintersManager()->AddPrinter(printer, chromeos::PrinterClass::kSaved);

  SetCustomArg(kName);
  ASSERT_TRUE(RunExtensionSubtest("printing", "get_printers.html"));
}

IN_PROC_BROWSER_TEST_F(PrintingApiTest, GetPrinterInfo) {
  AddAvailablePrinter(
      kId, std::make_unique<printing::PrinterSemanticCapsAndDefaults>());
  ASSERT_TRUE(RunExtensionSubtest("printing", "get_printer_info.html"));
}

// Verifies that:
// a) PrintingHooksDelegate substitutes corresponding Blob UUID and DCHECK
// doesn't fail.
// b) Whole API arguments handling pipeline works correctly.
// We use fake version of PrintJobController because we don't have a mock
// version of PrintingContext which is required to handle sending print job to
// the printer.
IN_PROC_BROWSER_TEST_F(PrintingApiTest, SubmitJob) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  AddAvailablePrinter(kId, ConstructPrinterCapabilities());
  PrintingAPIHandler::Get(browser()->profile())
      ->SetPrintJobControllerForTesting(
          std::make_unique<FakePrintJobController>(GetPrintJobManager(),
                                                   GetPrintersManager()));
  base::AutoReset<bool> skip_confirmation_dialog_reset(
      PrintJobSubmitter::SkipConfirmationDialogForTesting());

  ASSERT_TRUE(RunExtensionSubtest("printing", "submit_job.html"));
}

// Verifies that:
// a) Cancel job request works smoothly.
// b) OnJobStatusChanged() events are dispatched correctly.
IN_PROC_BROWSER_TEST_F(PrintingApiTest, CancelJob) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  AddAvailablePrinter(kId, ConstructPrinterCapabilities());
  PrintingAPIHandler::Get(browser()->profile())
      ->SetPrintJobControllerForTesting(
          std::make_unique<FakePrintJobController>(GetPrintJobManager(),
                                                   GetPrintersManager()));
  base::AutoReset<bool> skip_confirmation_dialog_reset(
      PrintJobSubmitter::SkipConfirmationDialogForTesting());

  ASSERT_TRUE(RunExtensionSubtest("printing", "cancel_job.html"));
}

}  // namespace extensions

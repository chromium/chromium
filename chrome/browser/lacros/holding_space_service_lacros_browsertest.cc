// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/holding_space_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Mocks -----------------------------------------------------------------------

class MockHoldingSpaceService : public crosapi::mojom::HoldingSpaceService {
 public:
  // crosapi::mojom::HoldingSpaceService:
  MOCK_METHOD(void,
              AddPrintedPdf,
              (const base::FilePath& file_path, bool from_incognito_profile),
              (override));
};

}  // namespace

// HoldingSpaceServiceBrowserTest ----------------------------------------------

// Base class for tests of the holding space service.
class HoldingSpaceServiceBrowserTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // If the holding space service interface is not available on this version
    // of ash-chrome, this test suite will no-op.
    if (!IsServiceAvailable())
      return;

    // Replace the production holding space service with a mock for testing.
    mojo::Remote<crosapi::mojom::HoldingSpaceService>& remote =
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::HoldingSpaceService>();
    remote.reset();
    receiver_.Bind(remote.BindNewPipeAndPassReceiver());
  }

  // Returns whether the holding space service interface is available. It may
  // not be available on earlier versions of ash-chrome.
  bool IsServiceAvailable() const {
    chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
    return lacros_service &&
           lacros_service->IsAvailable<crosapi::mojom::HoldingSpaceService>();
  }

  // Returns a reference to the mocked holding space `service_`.
  testing::NiceMock<MockHoldingSpaceService>& service() { return service_; }

 private:
  testing::NiceMock<MockHoldingSpaceService> service_;
  mojo::Receiver<crosapi::mojom::HoldingSpaceService> receiver_{&service_};
};

// HoldingSpaceServicePrintToPdfIntegrationBrowserTest -------------------------

// Base class for tests of print-to-PDF integration with the holding space
// service. Parameterized by whether tests should use an incognito browser.
class HoldingSpaceServicePrintToPdfIntegrationBrowserTest
    : public HoldingSpaceServiceBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  // Starts a job to print an empty PDF to the specified `file_path`.
  // NOTE: This method will not return until the print job completes.
  void StartPrintToPdfAndWaitForSave(const std::u16string& job_title,
                                     const base::FilePath& file_path) {
    base::RunLoop run_loop;
    pdf_printer_handler_->SetPdfSavedClosureForTesting(run_loop.QuitClosure());
    pdf_printer_handler_->SetPrintToPdfPathForTesting(file_path);

    pdf_printer_handler_->StartPrint(
        job_title,
        /*settings=*/base::Value::Dict(),
        base::MakeRefCounted<base::RefCountedString>(std::string()),
        /*callback=*/base::DoNothing());
    run_loop.Run();
  }

  // Returns true if the test should use an incognito browser, false otherwise.
  bool UseIncognitoBrowser() const { return GetParam(); }

 private:
  // HoldingSpaceServiceBrowserTest:
  void SetUpOnMainThread() override {
    HoldingSpaceServiceBrowserTest::SetUpOnMainThread();

    // If the holding space service interface is not available on this version
    // of ash-chrome, this test suite will no-op.
    if (!IsServiceAvailable())
      return;

    // Create the PDF printer handler.
    Browser* browser = GetBrowserForPdfPrinterHandler();
    pdf_printer_handler_ = std::make_unique<printing::PdfPrinterHandler>(
        browser->profile(), browser->tab_strip_model()->GetActiveWebContents(),
        /*sticky_settings=*/nullptr);
  }

  Browser* GetBrowserForPdfPrinterHandler() {
    if (!UseIncognitoBrowser())
      return browser();
    if (!incognito_browser_)
      incognito_browser_ = CreateIncognitoBrowser(browser()->profile());
    return incognito_browser_;
  }

  std::unique_ptr<printing::PdfPrinterHandler> pdf_printer_handler_;
  raw_ptr<Browser, DanglingUntriaged> incognito_browser_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceServicePrintToPdfIntegrationBrowserTest,
                         testing::Bool());

// Verifies that print-to-PDF adds an associated item to holding space.
IN_PROC_BROWSER_TEST_P(HoldingSpaceServicePrintToPdfIntegrationBrowserTest,
                       AddPrintedPdfItem) {
  // If holding space service interface is not available on this version of
  // ash-chrome, this test suite will no-op.
  if (!IsServiceAvailable())
    return;

  // Setup a temporary directory.
  base::ScopedAllowBlockingForTesting scoped_allow_blocking_for_testing;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Start and wait for a job to print an empty PDF to `file_path`. It is
  // expected that this will result in an interaction with the holding space
  // `service()` to create a printed PDF item.
  base::FilePath file_path = temp_dir.GetPath().Append("foo.pdf");
  EXPECT_CALL(service(), AddPrintedPdf(testing::Eq(file_path),
                                       /*from_incognito_profile=*/testing::Eq(
                                           UseIncognitoBrowser())));
  StartPrintToPdfAndWaitForSave(u"job_title", file_path);
}

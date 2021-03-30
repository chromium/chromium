// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "chrome/services/printing/print_backend_service_test_impl.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

using ::testing::UnorderedElementsAreArray;

namespace {

constexpr char kDefaultPrinterName[] = "default-test-printer";
constexpr char kAnotherPrinterName[] = "another-test-printer";
constexpr char kInvalidPrinterName[] = "invalid-test-printer";

const PrinterBasicInfo kDefaultPrinterInfo(
    /*printer_name=*/kDefaultPrinterName,
    /*display_name=*/"default test printer",
    /*printer_description=*/"Default printer for testing.",
    /*printer_status=*/0,
    /*is_default=*/true,
    /*options=*/{});
const PrinterBasicInfo kAnotherPrinterInfo(
    /*printer_name=*/kAnotherPrinterName,
    /*display_name=*/"another test printer",
    /*printer_description=*/"Another printer for testing.",
    /*printer_status=*/5,
    /*is_default=*/false,
    /*options=*/{});

constexpr int32_t kCopiesMax = 123;

}  // namespace

class PrintBackendBrowserTest : public InProcessBrowserTest {
 public:
  PrintBackendBrowserTest() = default;
  ~PrintBackendBrowserTest() override = default;

  void LaunchUninitialized() {
    print_backend_service_ =
        PrintBackendServiceTestImpl::LaunchUninitialized(remote_);
  }

  // Initialize and load the backend service with some test print drivers.
  void LaunchService() {
    print_backend_service_ = PrintBackendServiceTestImpl::LaunchForTesting(
        remote_, test_print_backend_);
  }

  // Load the test backend with a default printer driver.
  void AddDefaultPrinter() {
    // Only explicitly specify capabilities that we pay attention to in the
    // tests.
    auto default_caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
    default_caps->copies_max = kCopiesMax;
    test_print_backend_->AddValidPrinter(
        kDefaultPrinterName, std::move(default_caps),
        std::make_unique<PrinterBasicInfo>(kDefaultPrinterInfo));
  }

  // Load the test backend with another (non-default) printer.
  void AddAnotherPrinter() {
    test_print_backend_->AddValidPrinter(
        kAnotherPrinterName, std::make_unique<PrinterSemanticCapsAndDefaults>(),
        std::make_unique<PrinterBasicInfo>(kAnotherPrinterInfo));
  }

  // Public callbacks used by tests.
  void OnDidEnumeratePrinters(base::Optional<PrinterList>* capture_printer_list,
                              const base::Optional<PrinterList>& printer_list) {
    *capture_printer_list = printer_list;
    CheckForQuit();
  }

  void OnDidGetDefaultPrinterName(
      base::Optional<std::string>* capture_printer_name,
      const base::Optional<std::string>& printer_name) {
    *capture_printer_name = printer_name;
    CheckForQuit();
  }

  void OnDidGetPrinterSemanticCapsAndDefaults(
      base::Optional<PrinterSemanticCapsAndDefaults>* capture_printer_caps,
      const base::Optional<PrinterSemanticCapsAndDefaults>& printer_caps) {
    *capture_printer_caps = printer_caps;
    CheckForQuit();
  }

  void OnDidFetchCapabilities(
      base::Optional<PrinterBasicInfo>* capture_printer_info,
      base::Optional<PrinterSemanticCapsAndDefaults::Papers>*
          capture_user_defined_papers,
      base::Optional<PrinterSemanticCapsAndDefaults>* capture_printer_caps,
      const base::Optional<PrinterBasicInfo>& printer_info,
      const base::Optional<PrinterSemanticCapsAndDefaults::Papers>&
          user_defined_papers,
      const base::Optional<PrinterSemanticCapsAndDefaults>& printer_caps) {
    *capture_printer_info = printer_info;
    *capture_user_defined_papers = user_defined_papers;
    *capture_printer_caps = printer_caps;
    CheckForQuit();
  }

  // The following are helper functions for having a wait loop in the test and
  // exit when expected messages are received.  Expect to only have to wait for
  // one message.
  void WaitUntilCallbackReceived() {
    // If callback happened before getting here, then no need to wait.
    if (!received_message_) {
      base::RunLoop run_loop;
      quit_callback_ = run_loop.QuitClosure();
      run_loop.Run();
    }

    // Reset for possible subsequent test.
    received_message_ = false;
  }

  void CheckForQuit() {
    received_message_ = true;
    if (quit_callback_)
      std::move(quit_callback_).Run();
  }

  // Get the print backend service being tested.
  mojom::PrintBackendService* GetPrintBackendService() const {
    return print_backend_service_.get();
  }

 private:
  bool received_message_ = false;
  base::OnceClosure quit_callback_;

  mojo::Remote<mojom::PrintBackendService> remote_;
  scoped_refptr<TestPrintBackend> test_print_backend_ =
      base::MakeRefCounted<TestPrintBackend>();
  std::unique_ptr<PrintBackendServiceTestImpl> print_backend_service_;
};

// A print backend service requires initialization prior to being used for a
// query/command.  Verify that a query fails if one tries to use a new service
// without having performed initialization.
IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, FailWithoutInit) {
  // Launch the service, but without initializing to desired locale.
  LaunchUninitialized();

  base::Optional<std::string> default_printer_name;
  base::Optional<PrinterSemanticCapsAndDefaults> printer_caps;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->GetDefaultPrinterName(
      base::BindOnce(&PrintBackendBrowserTest::OnDidGetDefaultPrinterName,
                     base::Unretained(this), &default_printer_name));
  WaitUntilCallbackReceived();
  EXPECT_FALSE(default_printer_name.has_value());

  GetPrintBackendService()->GetPrinterSemanticCapsAndDefaults(
      kDefaultPrinterName,
      base::BindOnce(
          &PrintBackendBrowserTest::OnDidGetPrinterSemanticCapsAndDefaults,
          base::Unretained(this), &printer_caps));
  WaitUntilCallbackReceived();
  EXPECT_FALSE(printer_caps.has_value());
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, EnumeratePrinters) {
  LaunchService();
  AddDefaultPrinter();
  AddAnotherPrinter();

  const PrinterList kPrinterListExpected = {kDefaultPrinterInfo,
                                            kAnotherPrinterInfo};

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  base::Optional<PrinterList> printer_list;
  GetPrintBackendService()->EnumeratePrinters(
      base::BindOnce(&PrintBackendBrowserTest::OnDidEnumeratePrinters,
                     base::Unretained(this), &printer_list));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(printer_list.has_value());
  EXPECT_THAT(printer_list.value(),
              UnorderedElementsAreArray(kPrinterListExpected));
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, GetDefaultPrinterName) {
  LaunchService();
  AddDefaultPrinter();

  base::Optional<std::string> default_printer_name;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->GetDefaultPrinterName(
      base::BindOnce(&PrintBackendBrowserTest::OnDidGetDefaultPrinterName,
                     base::Unretained(this), &default_printer_name));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(default_printer_name.has_value());
  EXPECT_EQ(default_printer_name.value(), kDefaultPrinterName);
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest,
                       GetPrinterSemanticCapsAndDefaults) {
  LaunchService();
  AddDefaultPrinter();

  base::Optional<PrinterSemanticCapsAndDefaults> printer_caps;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->GetPrinterSemanticCapsAndDefaults(
      kDefaultPrinterName,
      base::BindOnce(
          &PrintBackendBrowserTest::OnDidGetPrinterSemanticCapsAndDefaults,
          base::Unretained(this), &printer_caps));
  WaitUntilCallbackReceived();
  EXPECT_TRUE(printer_caps.has_value());
  EXPECT_EQ(printer_caps->copies_max, kCopiesMax);

  // Requesting for an invalid printer should not return capabilities.
  GetPrintBackendService()->GetPrinterSemanticCapsAndDefaults(
      kInvalidPrinterName,
      base::BindOnce(
          &PrintBackendBrowserTest::OnDidGetPrinterSemanticCapsAndDefaults,
          base::Unretained(this), &printer_caps));
  WaitUntilCallbackReceived();
  EXPECT_FALSE(printer_caps.has_value());
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, FetchCapabilities) {
  LaunchService();
  AddDefaultPrinter();

  base::Optional<PrinterBasicInfo> printer_info;
  base::Optional<PrinterSemanticCapsAndDefaults::Papers> user_defined_papers;
  base::Optional<PrinterSemanticCapsAndDefaults> printer_caps;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->FetchCapabilities(
      kDefaultPrinterName,
      base::BindOnce(&PrintBackendBrowserTest::OnDidFetchCapabilities,
                     base::Unretained(this), &printer_info,
                     &user_defined_papers, &printer_caps));
  WaitUntilCallbackReceived();
  EXPECT_TRUE(printer_info.has_value());
  EXPECT_TRUE(user_defined_papers.has_value());
  EXPECT_TRUE(printer_caps.has_value());
  EXPECT_TRUE(printer_info->is_default);
  EXPECT_EQ(printer_caps->copies_max, kCopiesMax);

  // Requesting for an invalid printer should not return capabilities.
  GetPrintBackendService()->FetchCapabilities(
      kInvalidPrinterName,
      base::BindOnce(&PrintBackendBrowserTest::OnDidFetchCapabilities,
                     base::Unretained(this), &printer_info,
                     &user_defined_papers, &printer_caps));
  WaitUntilCallbackReceived();
  EXPECT_FALSE(printer_info.has_value());
  EXPECT_FALSE(user_defined_papers.has_value());
  EXPECT_FALSE(printer_caps.has_value());
}

}  // namespace printing

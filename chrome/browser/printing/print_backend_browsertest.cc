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
#include "base/run_loop.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

using ::testing::UnorderedElementsAreArray;

namespace {

constexpr char kDefaultPrinterName[] = "default-test-printer";
constexpr char kAnotherPrinterName[] = "another-test-printer";
constexpr char kInvalidPrinterName[] = "invalid-test-printer";
constexpr char kAccessDeniedPrinterName[] = "access-denied-test-printer";

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
        remote_, test_print_backend_, /*sandboxed=*/true);
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

  void AddAccessDeniedPrinter() {
    test_print_backend_->AddAccessDeniedPrinter(kAccessDeniedPrinterName);
  }

  // Public callbacks used by tests.
  void OnDidEnumeratePrinters(mojom::PrinterListResultPtr& capture_printer_list,
                              mojom::PrinterListResultPtr printer_list) {
    capture_printer_list = std::move(printer_list);
    CheckForQuit();
  }

  void OnDidGetDefaultPrinterName(
      mojom::DefaultPrinterNameResultPtr& capture_printer_name,
      mojom::DefaultPrinterNameResultPtr printer_name) {
    capture_printer_name = std::move(printer_name);
    CheckForQuit();
  }

  void OnDidGetPrinterSemanticCapsAndDefaults(
      mojom::PrinterSemanticCapsAndDefaultsResultPtr& capture_printer_caps,
      mojom::PrinterSemanticCapsAndDefaultsResultPtr printer_caps) {
    capture_printer_caps = std::move(printer_caps);
    CheckForQuit();
  }

  void OnDidFetchCapabilities(
      mojom::PrinterCapsAndInfoResultPtr& capture_caps_and_info,
      mojom::PrinterCapsAndInfoResultPtr caps_and_info) {
    capture_caps_and_info = std::move(caps_and_info);
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

  mojom::DefaultPrinterNameResultPtr default_printer_name;
  mojom::PrinterSemanticCapsAndDefaultsResultPtr printer_caps;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->GetDefaultPrinterName(
      base::BindOnce(&PrintBackendBrowserTest::OnDidGetDefaultPrinterName,
                     base::Unretained(this), std::ref(default_printer_name)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(default_printer_name->is_result_code());
  EXPECT_EQ(default_printer_name->get_result_code(),
            mojom::ResultCode::kFailed);

  GetPrintBackendService()->GetPrinterSemanticCapsAndDefaults(
      kDefaultPrinterName,
      base::BindOnce(
          &PrintBackendBrowserTest::OnDidGetPrinterSemanticCapsAndDefaults,
          base::Unretained(this), std::ref(printer_caps)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(printer_caps->is_result_code());
  EXPECT_EQ(printer_caps->get_result_code(), mojom::ResultCode::kFailed);
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, EnumeratePrinters) {
  LaunchService();
  AddDefaultPrinter();
  AddAnotherPrinter();

  const PrinterList kPrinterListExpected = {kDefaultPrinterInfo,
                                            kAnotherPrinterInfo};

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  mojom::PrinterListResultPtr printer_list;
  GetPrintBackendService()->EnumeratePrinters(
      base::BindOnce(&PrintBackendBrowserTest::OnDidEnumeratePrinters,
                     base::Unretained(this), std::ref(printer_list)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(printer_list->is_printer_list());
  EXPECT_THAT(printer_list->get_printer_list(),
              UnorderedElementsAreArray(kPrinterListExpected));
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, GetDefaultPrinterName) {
  LaunchService();
  AddDefaultPrinter();

  mojom::DefaultPrinterNameResultPtr default_printer_name;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->GetDefaultPrinterName(
      base::BindOnce(&PrintBackendBrowserTest::OnDidGetDefaultPrinterName,
                     base::Unretained(this), std::ref(default_printer_name)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(default_printer_name->is_default_printer_name());
  EXPECT_EQ(default_printer_name->get_default_printer_name(),
            kDefaultPrinterName);
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest,
                       GetPrinterSemanticCapsAndDefaults) {
  LaunchService();
  AddDefaultPrinter();

  mojom::PrinterSemanticCapsAndDefaultsResultPtr printer_caps;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->GetPrinterSemanticCapsAndDefaults(
      kDefaultPrinterName,
      base::BindOnce(
          &PrintBackendBrowserTest::OnDidGetPrinterSemanticCapsAndDefaults,
          base::Unretained(this), std::ref(printer_caps)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(printer_caps->is_printer_caps());
  EXPECT_EQ(printer_caps->get_printer_caps().copies_max, kCopiesMax);

  // Requesting for an invalid printer should not return capabilities.
  GetPrintBackendService()->GetPrinterSemanticCapsAndDefaults(
      kInvalidPrinterName,
      base::BindOnce(
          &PrintBackendBrowserTest::OnDidGetPrinterSemanticCapsAndDefaults,
          base::Unretained(this), std::ref(printer_caps)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(printer_caps->is_result_code());
  EXPECT_EQ(printer_caps->get_result_code(), mojom::ResultCode::kFailed);
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest,
                       GetPrinterSemanticCapsAndDefaultsAccessDenied) {
  LaunchService();
  AddAccessDeniedPrinter();

  mojom::PrinterSemanticCapsAndDefaultsResultPtr printer_caps;

  // Requesting for a printer which requires elevated privileges should not
  // return capabilities, and should indicate that access was denied.
  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->GetPrinterSemanticCapsAndDefaults(
      kAccessDeniedPrinterName,
      base::BindOnce(
          &PrintBackendBrowserTest::OnDidGetPrinterSemanticCapsAndDefaults,
          base::Unretained(this), std::ref(printer_caps)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(printer_caps->is_result_code());
  EXPECT_EQ(printer_caps->get_result_code(), mojom::ResultCode::kAccessDenied);
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, FetchCapabilities) {
  LaunchService();
  AddDefaultPrinter();

  mojom::PrinterCapsAndInfoResultPtr caps_and_info;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->FetchCapabilities(
      kDefaultPrinterName,
      base::BindOnce(&PrintBackendBrowserTest::OnDidFetchCapabilities,
                     base::Unretained(this), std::ref(caps_and_info)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(caps_and_info->is_printer_caps_and_info());
  EXPECT_EQ(caps_and_info->get_printer_caps_and_info()->printer_caps.copies_max,
            kCopiesMax);

  // Requesting for an invalid printer should not return capabilities.
  GetPrintBackendService()->FetchCapabilities(
      kInvalidPrinterName,
      base::BindOnce(&PrintBackendBrowserTest::OnDidFetchCapabilities,
                     base::Unretained(this), std::ref(caps_and_info)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(caps_and_info->is_result_code());
  EXPECT_EQ(caps_and_info->get_result_code(), mojom::ResultCode::kFailed);
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, FetchCapabilitiesAccessDenied) {
  LaunchService();
  AddAccessDeniedPrinter();

  mojom::PrinterCapsAndInfoResultPtr caps_and_info;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->FetchCapabilities(
      kAccessDeniedPrinterName,
      base::BindOnce(&PrintBackendBrowserTest::OnDidFetchCapabilities,
                     base::Unretained(this), std::ref(caps_and_info)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(caps_and_info->is_result_code());
  EXPECT_EQ(caps_and_info->get_result_code(), mojom::ResultCode::kAccessDenied);
}

}  // namespace printing

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "chrome/services/printing/print_backend_service_impl.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "printing/backend/test_print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

constexpr char kDefaultPrinterName[] = "default-test-printer";

}  // namespace

// PrintBackendServiceTestImpl uses a TestPrintBackend to enable testing of the
// PrintBackendService without relying upon the presence of real printer
// drivers.
class PrintBackendServiceTestImpl : public PrintBackendServiceImpl {
 public:
  explicit PrintBackendServiceTestImpl(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver)
      : PrintBackendServiceImpl(std::move(receiver)) {}
  PrintBackendServiceTestImpl(const PrintBackendServiceTestImpl&) = delete;
  PrintBackendServiceTestImpl& operator=(const PrintBackendServiceTestImpl&) =
      delete;
  ~PrintBackendServiceTestImpl() override = default;

  // Overrides which need special handling for using `test_print_backend_`.
  void Init(const std::string& locale) override {
    test_print_backend_ = base::MakeRefCounted<TestPrintBackend>();
    print_backend_ = test_print_backend_;
  }

 private:
  friend class PrintBackendBrowserTest;

  scoped_refptr<TestPrintBackend> test_print_backend_;
};

class PrintBackendBrowserTest : public InProcessBrowserTest {
 public:
  PrintBackendBrowserTest() = default;
  ~PrintBackendBrowserTest() override = default;

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();

    // Launch the service, and bind the testing interface to it.
    mojo::PendingReceiver<mojom::PrintBackendService> receiver =
        mojo::PendingRemote<mojom::PrintBackendService>()
            .InitWithNewPipeAndPassReceiver();
    print_backend_service_ =
        std::make_unique<PrintBackendServiceTestImpl>(std::move(receiver));
  }

  // Initialize and load the backend service with some test print drivers.
  void DoInitAndSetupTestData() {
    print_backend_service_->Init(/*locale=*/"");
    print_backend_service_->test_print_backend_->SetDefaultPrinterName(
        kDefaultPrinterName);
  }

  // Public callbacks used by tests.
  void OnDidGetDefaultPrinterName(
      base::Optional<std::string>* capture_printer_name,
      const base::Optional<std::string>& printer_name) {
    *capture_printer_name = printer_name;
    CheckForQuit();
  }

  // The following are helper functions for having a wait loop in the test and
  // exit when expected messages are received.
  void WaitUntilCallbackReceived() {
    if (received_message_) {
      // Callback happened before even needing to wait.
      return;
    }

    base::RunLoop run_loop;
    quit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
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

  std::unique_ptr<PrintBackendServiceTestImpl> print_backend_service_;
};

// A print backend service requires initialization prior to being used for a
// query/command.  Verify that a query fails if one tries to use a new service
// without having performed initialization.
IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, FailWithoutInit) {
  base::Optional<std::string> default_printer_name;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->GetDefaultPrinterName(
      base::BindOnce(&PrintBackendBrowserTest::OnDidGetDefaultPrinterName,
                     base::Unretained(this), &default_printer_name));
  WaitUntilCallbackReceived();
  EXPECT_FALSE(default_printer_name.has_value());
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, GetDefaultPrinterName) {
  base::Optional<std::string> default_printer_name;

  DoInitAndSetupTestData();

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->GetDefaultPrinterName(
      base::BindOnce(&PrintBackendBrowserTest::OnDidGetDefaultPrinterName,
                     base::Unretained(this), &default_printer_name));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(default_printer_name.has_value());
  EXPECT_EQ(default_printer_name.value(), kDefaultPrinterName);
}

}  // namespace printing

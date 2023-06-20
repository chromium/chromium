// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/print_management/backend/print_management_handler.h"

#include <memory>

#include "ash/webui/print_management/backend/print_management_delegate.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::printing::printing_manager {

namespace {

class FakePrintManagementDelegate : public PrintManagementDelegate {
 public:
  FakePrintManagementDelegate() = default;
  ~FakePrintManagementDelegate() override = default;

  // PrintManagementDelegate:
  void LaunchPrinterSettings() override { launch_printer_settings_count_++; }

  int launch_printer_settings_count() const {
    return launch_printer_settings_count_;
  }

 private:
  int launch_printer_settings_count_ = 0;
};

}  // namespace

class PrintManagementHandlerTest : public testing::Test {
 public:
  PrintManagementHandlerTest() {
    auto delegate = std::make_unique<FakePrintManagementDelegate>();
    delegate_ = delegate.get();
    handler_ = std::make_unique<PrintManagementHandler>(std::move(delegate));
  }
  ~PrintManagementHandlerTest() override = default;

  FakePrintManagementDelegate* delegate() { return delegate_; }
  PrintManagementHandler* handler() { return handler_.get(); }

 private:
  raw_ptr<FakePrintManagementDelegate, ExperimentalAsh> delegate_;
  std::unique_ptr<PrintManagementHandler> handler_;
};

// Verifies handler uses delegate to attempt to open printer settings.
TEST_F(PrintManagementHandlerTest, LaunchPrinterSettingsCallsDelegate) {
  EXPECT_EQ(0, delegate()->launch_printer_settings_count());

  handler()->LaunchPrinterSettings();

  EXPECT_EQ(1, delegate()->launch_printer_settings_count());
}

}  // namespace ash::printing::printing_manager

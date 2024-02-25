// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/print_management/backend/print_management_handler.h"

#include <memory>

#include "ash/webui/print_management/backend/print_management_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::printing::printing_manager {

namespace {

constexpr char kRecordUserActionMetric[] =
    "ChromeOS.PrintManagement.PrinterSettingsLaunchSource";

using chromeos::printing::printing_manager::mojom::LaunchSource;

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
  raw_ptr<FakePrintManagementDelegate, DanglingUntriaged> delegate_;
  std::unique_ptr<PrintManagementHandler> handler_;
};

// Verifies handler uses delegate to attempt to open printer settings.
TEST_F(PrintManagementHandlerTest, LaunchPrinterSettingsCallsDelegate) {
  EXPECT_EQ(0, delegate()->launch_printer_settings_count());

  handler()->LaunchPrinterSettings(LaunchSource::kEmptyStateButton);

  EXPECT_EQ(1, delegate()->launch_printer_settings_count());
}

// Verifies handler records expected metrics when launch printer settings
// called.
TEST_F(PrintManagementHandlerTest, LaunchPrinterSettingsTriggersMetric) {
  base::HistogramTester tester;
  tester.ExpectBucketCount(kRecordUserActionMetric,
                           LaunchSource::kEmptyStateButton,
                           /*expected_count=*/0);
  tester.ExpectBucketCount(kRecordUserActionMetric, LaunchSource::kHeaderButton,
                           /*expected_count=*/0);

  // Simulate launch from empty state 'manage printers' button.
  handler()->LaunchPrinterSettings(LaunchSource::kEmptyStateButton);

  tester.ExpectBucketCount(kRecordUserActionMetric,
                           LaunchSource::kEmptyStateButton,
                           /*expected_count=*/1);
  tester.ExpectBucketCount(kRecordUserActionMetric, LaunchSource::kHeaderButton,
                           /*expected_count=*/0);

  // Simulate launch from header 'manage printers' button.
  handler()->LaunchPrinterSettings(LaunchSource::kHeaderButton);

  tester.ExpectBucketCount(kRecordUserActionMetric,
                           LaunchSource::kEmptyStateButton,
                           /*expected_count=*/1);
  tester.ExpectBucketCount(kRecordUserActionMetric, LaunchSource::kHeaderButton,
                           /*expected_count=*/1);
}

}  // namespace ash::printing::printing_manager

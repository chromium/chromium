// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/report_controller_initializer/report_controller_initializer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class ReportControllerInitializerValidateSegment
    : public testing::TestWithParam<std::tuple<policy::DeviceMode,
                                               policy::MarketSegment,
                                               report::MarketSegment>> {
 public:
  void SetUp() override {
    // Initialize CrOS device settings for ReportControllerInitializer.
    DeviceSettingsService::Initialize();

    // Create ReportControllerInitializer for unit testing.
    report_controller_initializer_ =
        std::make_unique<ReportControllerInitializer>();
  }

  void TearDown() override {
    // Destruct ReportControllerInitializer before cleaning up dependencies.
    report_controller_initializer_.reset();

    // Clean up CrOS device settings after testing.
    DeviceSettingsService::Shutdown();
  }

  ReportControllerInitializer* GetReportControllerInitializer() {
    return report_controller_initializer_.get();
  }

 protected:
  report::MarketSegment GetMarketSegmentForTesting(
      const policy::DeviceMode& device_mode,
      const policy::MarketSegment& device_market_segment) {
    return report_controller_initializer_->GetMarketSegmentForTesting(
        device_mode, device_market_segment);
  }

 private:
  std::unique_ptr<ReportControllerInitializer> report_controller_initializer_;
};

TEST_P(ReportControllerInitializerValidateSegment, ValidateSegment) {
  auto [device_mode, device_market_segment, expected_segment] = GetParam();
  ASSERT_EQ(expected_segment,
            GetMarketSegmentForTesting(device_mode, device_market_segment));
}

INSTANTIATE_TEST_SUITE_P(
    ReportControllerInitializerValidateSegmentTests,
    ReportControllerInitializerValidateSegment,
    testing::Values(
        // Unknown DeviceMode
        std::make_tuple(policy::DeviceMode::DEVICE_MODE_PENDING,
                        policy::MarketSegment::UNKNOWN,
                        report::MARKET_SEGMENT_UNKNOWN),
        // Consumer DeviceMode
        std::make_tuple(policy::DeviceMode::DEVICE_MODE_CONSUMER,
                        policy::MarketSegment::UNKNOWN,
                        report::MARKET_SEGMENT_CONSUMER),
        // Demo Enterprise DeviceMode
        std::make_tuple(policy::DeviceMode::DEVICE_MODE_DEMO,
                        policy::MarketSegment::ENTERPRISE,
                        report::MARKET_SEGMENT_ENTERPRISE_DEMO),
        // Enterprise DeviceMode Enterprise Segment
        std::make_tuple(policy::DeviceMode::DEVICE_MODE_ENTERPRISE,
                        policy::MarketSegment::ENTERPRISE,
                        report::MARKET_SEGMENT_ENTERPRISE),
        // Enterprise DeviceMode Education Segment
        std::make_tuple(policy::DeviceMode::DEVICE_MODE_ENTERPRISE,
                        policy::MarketSegment::EDUCATION,
                        report::MARKET_SEGMENT_EDUCATION),
        // Enterprise DeviceMode Unknown Segment
        std::make_tuple(policy::DeviceMode::DEVICE_MODE_ENTERPRISE,
                        policy::MarketSegment::UNKNOWN,
                        report::MARKET_SEGMENT_ENTERPRISE_ENROLLED_BUT_UNKNOWN),
        // Unknown DeviceMode And Segment
        std::make_tuple(policy::DeviceMode::DEVICE_MODE_NOT_SET,
                        policy::MarketSegment::UNKNOWN,
                        report::MARKET_SEGMENT_UNKNOWN)));

}  // namespace ash

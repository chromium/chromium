// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_config_manager.h"

#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ExtensionInfo =
    safe_browsing::ExtensionTelemetryReportRequest_ExtensionInfo;
using TelemetryReport = safe_browsing::ExtensionTelemetryReportRequest;

namespace safe_browsing {

namespace {

constexpr uint32_t kMaxUintValue = 0xffffffff;
constexpr uint64_t kMaxUint64Value = 0xffffffffffffffff;
constexpr uint32_t kDefaultReportingInterval = 3600u;
constexpr uint32_t kDefaultWritesPerInterval = 1u;
constexpr uint32_t kDefaultVersion = 0;
constexpr uint32_t kReportingInterval = 500u;
constexpr uint32_t kWritesPerInterval = 4u;
constexpr uint32_t kVersion = 3;
constexpr uint64_t kExtension1SignalEnables = 0x2B;
constexpr uint64_t kExtension2SignalEnables = 0xfffffffffffffff5;
constexpr char kExtension1[] = "extension1";
constexpr char kExtension2[] = "extension2";

}  // namespace

class ExtensionTelemetryConfigManagerTest : public ::testing::Test {
 protected:
  ExtensionTelemetryConfigManagerTest();

  void SetUp() override { ASSERT_NE(config_manager_, nullptr); }

  void ReloadConfig();
  void InitConfig();
  void InitConfigWithMaxValues();

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<ExtensionTelemetryConfigManager> config_manager_;
};

ExtensionTelemetryConfigManagerTest::ExtensionTelemetryConfigManagerTest()
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
      config_manager_(std::make_unique<ExtensionTelemetryConfigManager>(
          profile_.GetPrefs())) {}

void ExtensionTelemetryConfigManagerTest::InitConfigWithMaxValues() {
  ExtensionTelemetryReportResponse::Configuration config;
  config.set_configuration_version(kMaxUintValue);
  config.set_reporting_interval_seconds(kMaxUintValue);
  config.set_writes_per_interval(kMaxUintValue);
  safe_browsing::ExtensionTelemetryReportResponse_ExtensionParameters*
      extension = config.add_extension_parameters();
  extension->set_extension_id(kExtension1);
  extension->set_signal_enable_mask(kMaxUint64Value);
  config_manager_->SaveConfig(config);
}

void ExtensionTelemetryConfigManagerTest::InitConfig() {
  ExtensionTelemetryReportResponse::Configuration config;
  config.set_configuration_version(kVersion);
  config.set_reporting_interval_seconds(kReportingInterval);
  config.set_writes_per_interval(kWritesPerInterval);
  safe_browsing::ExtensionTelemetryReportResponse_ExtensionParameters*
      extension = config.add_extension_parameters();
  extension->set_extension_id(kExtension1);
  extension->set_signal_enable_mask(kExtension1SignalEnables);
  extension = config.add_extension_parameters();
  extension->set_extension_id(kExtension2);
  extension->set_signal_enable_mask(kExtension2SignalEnables);
  config_manager_->SaveConfig(config);
}

TEST_F(ExtensionTelemetryConfigManagerTest, InitializesSignalEnables) {
  // Standard configuration check of multiple signals.
  InitConfig();
  EXPECT_TRUE(config_manager_->IsSignalEnabled(
      kExtension1, ExtensionSignalType::kTabsExecuteScript));
  EXPECT_TRUE(config_manager_->IsSignalEnabled(
      kExtension2, ExtensionSignalType::kTabsExecuteScript));
  EXPECT_TRUE(config_manager_->IsSignalEnabled(
      kExtension2, ExtensionSignalType::kCookiesGetAll));
  EXPECT_TRUE(config_manager_->IsSignalEnabled(
      kExtension1, ExtensionSignalType::kRemoteHostContacted));
  EXPECT_FALSE(config_manager_->IsSignalEnabled(
      kExtension2, ExtensionSignalType::kRemoteHostContacted));
  EXPECT_FALSE(config_manager_->IsSignalEnabled(
      kExtension2, ExtensionSignalType::kPasswordReuse));
  EXPECT_EQ(config_manager_->GetSignalEnables(kExtension1),
            kExtension1SignalEnables);
  EXPECT_EQ(config_manager_->GetSignalEnables(kExtension2),
            kExtension2SignalEnables);
}

TEST_F(ExtensionTelemetryConfigManagerTest, InitializesWithMaxInputValues) {
  // Check that max input ranges are stored correctly.
  InitConfigWithMaxValues();
  EXPECT_EQ(config_manager_->GetWritesPerInterval(), kMaxUintValue);
  EXPECT_EQ(config_manager_->GetReportingInterval(), kMaxUintValue);
  EXPECT_EQ(config_manager_->GetConfigVersion(), kMaxUintValue);
  EXPECT_EQ(config_manager_->GetSignalEnables(kExtension1), kMaxUint64Value);
}

TEST_F(ExtensionTelemetryConfigManagerTest, SavesAndLoadsConfigFromPrefs) {
  // Store a config in prefs and reset the `config_manager_`.
  InitConfig();
  config_manager_ =
      std::make_unique<ExtensionTelemetryConfigManager>(profile_.GetPrefs());

  // Check the `config_manager_` doesn't have a loaded config and returns
  // default values.
  EXPECT_EQ(config_manager_->GetWritesPerInterval(), kDefaultWritesPerInterval);
  EXPECT_EQ(config_manager_->GetReportingInterval(), kDefaultReportingInterval);
  EXPECT_EQ(config_manager_->GetConfigVersion(), kDefaultVersion);

  // Now load the saved config from prefs.
  config_manager_->LoadConfig();

  // Check that `config_manager_` has the config values loaded from prefs.
  EXPECT_EQ(config_manager_->GetWritesPerInterval(), kWritesPerInterval);
  EXPECT_EQ(config_manager_->GetReportingInterval(), kReportingInterval);
  EXPECT_EQ(config_manager_->GetConfigVersion(), kVersion);
  EXPECT_TRUE(config_manager_->IsSignalEnabled(
      kExtension1, ExtensionSignalType::kRemoteHostContacted));
  EXPECT_FALSE(config_manager_->IsSignalEnabled(
      kExtension2, ExtensionSignalType::kRemoteHostContacted));
}

TEST_F(ExtensionTelemetryConfigManagerTest,
       ReturnsDefaultValuesIfNoPrefsDataPresent) {
  // Test to ensure that all signals are enabled if no configuration
  // is found and default values are used.
  EXPECT_EQ(config_manager_->GetWritesPerInterval(), kDefaultWritesPerInterval);
  EXPECT_EQ(config_manager_->GetReportingInterval(), kDefaultReportingInterval);
  EXPECT_EQ(config_manager_->GetConfigVersion(), kDefaultVersion);
  EXPECT_TRUE(config_manager_->IsSignalEnabled(
      kExtension1, ExtensionSignalType::kCookiesGetAll));
  EXPECT_TRUE(config_manager_->IsSignalEnabled(
      kExtension2, ExtensionSignalType::kCookiesGetAll));
  EXPECT_TRUE(config_manager_->IsSignalEnabled(
      kExtension1, ExtensionSignalType::kTabsExecuteScript));
  EXPECT_TRUE(config_manager_->IsSignalEnabled(
      kExtension2, ExtensionSignalType::kTabsExecuteScript));
  EXPECT_TRUE(config_manager_->IsSignalEnabled(
      kExtension2, ExtensionSignalType::kCookiesGet));
}
}  // namespace safe_browsing

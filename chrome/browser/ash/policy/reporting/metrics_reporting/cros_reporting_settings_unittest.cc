// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_reporting_settings.h"

#include <string>

#include "base/callback_list.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Not;

namespace reporting {

TEST(CrosReportingSettingsTest, InvalidPath) {
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings;
  CrosReportingSettings cros_reporting_settings;
  bool bool_value = false;
  int int_value = -1;

  // Getting boolean from an integer path is not valid.
  EXPECT_FALSE(cros_reporting_settings.GetBoolean(ash::kReportUploadFrequency,
                                                  &bool_value));
  EXPECT_FALSE(bool_value);
  // Getting integer from a boolean path is not valid.
  EXPECT_FALSE(cros_reporting_settings.GetInteger(
      ash::kReportDeviceNetworkStatus, &int_value));
  EXPECT_EQ(int_value, -1);
}

TEST(CrosReportingSettingsTest, GetBoolean) {
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings;
  CrosReportingSettings cros_reporting_settings;
  bool bool_value;

  scoped_testing_cros_settings.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkStatus, true);
  bool is_valid = cros_reporting_settings.GetBoolean(
      ash::kReportDeviceNetworkStatus, &bool_value);

  ASSERT_TRUE(is_valid);
  EXPECT_TRUE(bool_value);
}

TEST(CrosReportingSettingsTest, GetInteger) {
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings;
  CrosReportingSettings cros_reporting_settings;
  int int_value = -1;

  scoped_testing_cros_settings.device_settings()->SetInteger(
      ash::kReportUploadFrequency, 100);
  cros_reporting_settings.GetInteger(ash::kReportUploadFrequency, &int_value);

  EXPECT_EQ(int_value, 100);
}

TEST(CrosReportingSettingsTest, GetList) {
  ::ash::ScopedTestingCrosSettings scoped_testing_cros_settings;
  CrosReportingSettings cros_reporting_settings;

  static constexpr char kListSettingValue[] = "network_telemetry";
  base::Value::List list_setting;
  list_setting.Append(kListSettingValue);
  scoped_testing_cros_settings.device_settings()->Set(
      ::ash::kReportDeviceSignalStrengthEventDrivenTelemetry,
      base::Value(std::move(list_setting)));

  const base::Value::List* list_value = nullptr;
  ASSERT_TRUE(cros_reporting_settings.GetList(
      ::ash::kReportDeviceSignalStrengthEventDrivenTelemetry, &list_value));
  ASSERT_THAT(list_value, Not(IsNull()));
  ASSERT_THAT(list_value->size(), Eq(1uL));
  EXPECT_THAT(list_value->front().GetString(), Eq(kListSettingValue));
}

TEST(CrosReportingSettingsTest, GetReportingEnabled) {
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings;
  CrosReportingSettings cros_reporting_settings;
  bool enabled_value;

  scoped_testing_cros_settings.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkStatus, true);
  bool is_valid = cros_reporting_settings.GetReportingEnabled(
      ash::kReportDeviceNetworkStatus, &enabled_value);

  ASSERT_TRUE(is_valid);
  EXPECT_TRUE(enabled_value);
}

TEST(CrosReportingSettingsTest, GetReportingEnabled_InvalidType) {
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings;
  CrosReportingSettings cros_reporting_settings;
  bool enabled_value;

  scoped_testing_cros_settings.device_settings()->SetInteger(
      ash::kReportUploadFrequency, 100);
  bool is_valid = cros_reporting_settings.GetReportingEnabled(
      ash::kReportUploadFrequency, &enabled_value);

  ASSERT_FALSE(is_valid);
}

TEST(CrosReportingSettingsTest, AddSettingsObserver) {
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings;
  CrosReportingSettings cros_reporting_settings;
  bool callback_called;

  base::CallbackListSubscription bool_subscription =
      cros_reporting_settings.AddSettingsObserver(
          ash::kReportDeviceNetworkStatus,
          base::BindLambdaForTesting(
              [&callback_called]() { callback_called = true; }));
  scoped_testing_cros_settings.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkStatus, true);

  EXPECT_TRUE(callback_called);
}

TEST(CrosReportingSettingsTest, PrepareTrustedValues_InitiallyUntrusted) {
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings;
  CrosReportingSettings cros_reporting_settings;

  scoped_testing_cros_settings.device_settings()->SetTrustedStatus(
      ash::CrosSettingsProvider::TEMPORARILY_UNTRUSTED);
  bool callback_called = false;
  bool trusted =
      cros_reporting_settings.PrepareTrustedValues(base::BindLambdaForTesting(
          [&callback_called]() { callback_called = true; }));

  // Callback won't be called until the trusted status changes to TRUSTED.
  EXPECT_FALSE(callback_called);
  EXPECT_FALSE(trusted);

  // Callback will be called after setting the trusted status to TRUSTED.
  scoped_testing_cros_settings.device_settings()->SetTrustedStatus(
      ash::CrosSettingsProvider::TRUSTED);
  EXPECT_TRUE(callback_called);
}

TEST(CrosReportingSettingsTest, PrepareTrustedValues_InitiallyTrusted) {
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings;
  CrosReportingSettings cros_reporting_settings;

  scoped_testing_cros_settings.device_settings()->SetTrustedStatus(
      ash::CrosSettingsProvider::TRUSTED);
  bool callback_called = false;
  bool trusted =
      cros_reporting_settings.PrepareTrustedValues(base::BindLambdaForTesting(
          [&callback_called]() { callback_called = true; }));

  // Callback won't be called since the trusted status is already TRUSTED.
  EXPECT_FALSE(callback_called);
  EXPECT_TRUE(trusted);
}
}  // namespace reporting

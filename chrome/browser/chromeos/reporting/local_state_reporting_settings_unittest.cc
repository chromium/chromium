// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/local_state_reporting_settings.h"

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;

namespace reporting {
namespace {

constexpr char kSettingPath[] = "TestSetting";

class LocalStateReportingSettingsTest : public ::testing::Test {
 protected:
  TestingPrefServiceSimple* local_state() { return local_state_.Get(); }

 private:
  ::content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};

 protected:
  LocalStateReportingSettings local_state_reporting_settings_;
};

TEST_F(LocalStateReportingSettingsTest, InvalidIntegerPrefPath) {
  local_state()->registry()->RegisterIntegerPref(kSettingPath,
                                                 /*default_value=*/0);

  // Attempt to retrieve the integer reporting setting with incorrect data type
  // and confirm it fails.
  bool out_bool_value;
  ASSERT_FALSE(local_state_reporting_settings_.GetBoolean(kSettingPath,
                                                          &out_bool_value));
  ASSERT_FALSE(local_state_reporting_settings_.GetReportingEnabled(
      kSettingPath, &out_bool_value));
  const base::Value::List* out_list_value = nullptr;
  ASSERT_FALSE(
      local_state_reporting_settings_.GetList(kSettingPath, &out_list_value));
  EXPECT_THAT(out_list_value, IsNull());
}

TEST_F(LocalStateReportingSettingsTest, InvalidBooleanPrefPath) {
  local_state()->registry()->RegisterBooleanPref(kSettingPath,
                                                 /*default_value=*/true);

  // Attempt to retrieve the boolean reporting setting with incorrect data type
  // and confirm it fails.
  int out_int_value;
  ASSERT_FALSE(
      local_state_reporting_settings_.GetInteger(kSettingPath, &out_int_value));
  const base::Value::List* out_list_value = nullptr;
  ASSERT_FALSE(
      local_state_reporting_settings_.GetList(kSettingPath, &out_list_value));
  EXPECT_THAT(out_list_value, IsNull());
}

TEST_F(LocalStateReportingSettingsTest, InvalidListPrefPath) {
  local_state()->registry()->RegisterListPref(
      kSettingPath, /*default_value=*/base::Value::List());

  // Attempt to retrieve the list reporting setting with incorrect data type and
  // confirm it fails.
  bool out_bool_value;
  ASSERT_FALSE(local_state_reporting_settings_.GetBoolean(kSettingPath,
                                                          &out_bool_value));
  int out_int_value;
  EXPECT_FALSE(
      local_state_reporting_settings_.GetInteger(kSettingPath, &out_int_value));
}

TEST_F(LocalStateReportingSettingsTest, GetUnregisteredSetting) {
  bool out_bool_value;
  EXPECT_FALSE(local_state_reporting_settings_.GetBoolean(kSettingPath,
                                                          &out_bool_value));
  int out_int_value;
  EXPECT_FALSE(
      local_state_reporting_settings_.GetInteger(kSettingPath, &out_int_value));
  const base::Value::List* out_list_value = nullptr;
  EXPECT_FALSE(
      local_state_reporting_settings_.GetList(kSettingPath, &out_list_value));
}

TEST_F(LocalStateReportingSettingsTest, GetBoolean) {
  local_state()->registry()->RegisterBooleanPref(kSettingPath,
                                                 /*default_value=*/false);
  bool out_value = true;
  ASSERT_TRUE(
      local_state_reporting_settings_.GetBoolean(kSettingPath, &out_value));
  ASSERT_FALSE(out_value);

  // Update setting value and ensure the next fetch returns the updated value.
  local_state()->SetBoolean(kSettingPath, true);
  ASSERT_TRUE(
      local_state_reporting_settings_.GetBoolean(kSettingPath, &out_value));
  EXPECT_TRUE(out_value);
}

TEST_F(LocalStateReportingSettingsTest, GetReportingEnabled_Boolean) {
  local_state()->registry()->RegisterBooleanPref(kSettingPath,
                                                 /*default_value=*/false);
  bool out_value = true;
  ASSERT_TRUE(local_state_reporting_settings_.GetReportingEnabled(kSettingPath,
                                                                  &out_value));
  EXPECT_FALSE(out_value);

  // Update setting value and ensure the next fetch returns the updated value.
  local_state()->SetBoolean(kSettingPath, true);
  ASSERT_TRUE(
      local_state_reporting_settings_.GetBoolean(kSettingPath, &out_value));
  EXPECT_TRUE(out_value);
}

TEST_F(LocalStateReportingSettingsTest, GetInteger) {
  local_state()->registry()->RegisterIntegerPref(kSettingPath,
                                                 /*default_value=*/0);
  int out_value;
  ASSERT_TRUE(
      local_state_reporting_settings_.GetInteger(kSettingPath, &out_value));
  ASSERT_THAT(out_value, Eq(0));

  // Update setting value and ensure the next fetch returns the updated value.
  static constexpr int new_value = 1;
  local_state()->SetInteger(kSettingPath, new_value);
  ASSERT_TRUE(
      local_state_reporting_settings_.GetInteger(kSettingPath, &out_value));
  EXPECT_THAT(out_value, Eq(new_value));
}

TEST_F(LocalStateReportingSettingsTest, GetList) {
  local_state()->registry()->RegisterListPref(
      kSettingPath, /*default_value=*/base::Value::List());
  const base::Value::List* out_value;
  ASSERT_TRUE(
      local_state_reporting_settings_.GetList(kSettingPath, &out_value));
  ASSERT_THAT(out_value, NotNull());
  EXPECT_TRUE(out_value->empty());

  // Update setting value and ensure the next fetch returns the updated value.
  static constexpr char kListSettingItem[] = "item";
  local_state()->SetList(kSettingPath,
                         base::Value::List().Append(kListSettingItem));
  ASSERT_TRUE(
      local_state_reporting_settings_.GetList(kSettingPath, &out_value));
  ASSERT_THAT(out_value, NotNull());
  ASSERT_THAT(out_value->size(), Eq(1uL));
  EXPECT_THAT(out_value->front().GetString(), Eq(kListSettingItem));
}

TEST_F(LocalStateReportingSettingsTest, GetReportingEnabled_List) {
  local_state()->registry()->RegisterListPref(
      kSettingPath, /*default_value=*/base::Value::List());
  bool out_value = true;
  ASSERT_TRUE(local_state_reporting_settings_.GetReportingEnabled(kSettingPath,
                                                                  &out_value));
  EXPECT_FALSE(out_value);

  // Update setting value and ensure the next fetch returns the updated value.
  static constexpr char kListSettingItem[] = "item";
  local_state()->SetList(kSettingPath,
                         base::Value::List().Append(kListSettingItem));
  ASSERT_TRUE(local_state_reporting_settings_.GetReportingEnabled(kSettingPath,
                                                                  &out_value));
  EXPECT_TRUE(out_value);
}

TEST_F(LocalStateReportingSettingsTest, ObserveBooleanSetting) {
  local_state()->registry()->RegisterBooleanPref(kSettingPath,
                                                 /*default_value=*/false);
  bool callback_called = false;
  const auto callback_subscription =
      local_state_reporting_settings_.AddSettingsObserver(
          kSettingPath, base::BindLambdaForTesting(
                            [&callback_called]() { callback_called = true; }));

  // Update setting value and ensure callback was triggered.
  local_state()->SetBoolean(kSettingPath, true);
  ASSERT_TRUE(callback_called);
}

TEST_F(LocalStateReportingSettingsTest, ObserveIntegerSetting) {
  local_state()->registry()->RegisterIntegerPref(kSettingPath,
                                                 /*default_value=*/0);
  bool callback_called = false;
  const auto callback_subscription =
      local_state_reporting_settings_.AddSettingsObserver(
          kSettingPath, base::BindLambdaForTesting(
                            [&callback_called]() { callback_called = true; }));

  // Update setting value and ensure callback was triggered.
  local_state()->SetInteger(kSettingPath, 1);
  ASSERT_TRUE(callback_called);
}

TEST_F(LocalStateReportingSettingsTest, ObserveListSetting) {
  local_state()->registry()->RegisterListPref(
      kSettingPath, /*default_value=*/base::Value::List());
  bool callback_called = false;
  const auto callback_subscription =
      local_state_reporting_settings_.AddSettingsObserver(
          kSettingPath, base::BindLambdaForTesting(
                            [&callback_called]() { callback_called = true; }));

  // Update setting value and ensure callback was triggered.
  local_state()->SetList(kSettingPath, base::Value::List().Append("item"));
  ASSERT_TRUE(callback_called);
}

TEST_F(LocalStateReportingSettingsTest, MultipleSettingObservers) {
  local_state()->registry()->RegisterBooleanPref(kSettingPath,
                                                 /*default_value=*/false);
  int callback_trigger_count = 0;
  const auto callback_subscription_1 =
      local_state_reporting_settings_.AddSettingsObserver(
          kSettingPath, base::BindLambdaForTesting([&callback_trigger_count]() {
            callback_trigger_count++;
          }));
  const auto callback_subscription_2 =
      local_state_reporting_settings_.AddSettingsObserver(
          kSettingPath, base::BindLambdaForTesting([&callback_trigger_count]() {
            callback_trigger_count++;
          }));

  // Update setting value and ensure both callbacks were triggered.
  local_state()->SetBoolean(kSettingPath, true);
  ASSERT_THAT(callback_trigger_count, Eq(2));
}

}  // namespace
}  // namespace reporting

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/user_reporting_settings.h"

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;

namespace reporting {
namespace {

constexpr char kUserId[] = "123";
constexpr char kSettingPath[] = "TestSetting";

class UserReportingSettingsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kUserId);
    user_reporting_settings_ =
        std::make_unique<UserReportingSettings>(profile_->GetWeakPtr());
  }

  ::content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  std::unique_ptr<UserReportingSettings> user_reporting_settings_;
};

TEST_F(UserReportingSettingsTest, InvalidIntegerPrefPath) {
  profile_->GetTestingPrefService()->registry()->RegisterIntegerPref(
      kSettingPath, /*default_value=*/0);

  // Attempt to retrieve the integer reporting setting with incorrect data type
  // and confirm it fails.
  bool out_bool_value;
  ASSERT_FALSE(
      user_reporting_settings_->GetBoolean(kSettingPath, &out_bool_value));
  ASSERT_FALSE(user_reporting_settings_->GetReportingEnabled(kSettingPath,
                                                             &out_bool_value));
  const base::Value::List* out_list_value = nullptr;
  ASSERT_FALSE(
      user_reporting_settings_->GetList(kSettingPath, &out_list_value));
  EXPECT_THAT(out_list_value, IsNull());
}

TEST_F(UserReportingSettingsTest, InvalidBooleanPrefPath) {
  profile_->GetTestingPrefService()->registry()->RegisterBooleanPref(
      kSettingPath, /*default_value=*/true);

  // Attempt to retrieve the boolean reporting setting with incorrect data type
  // and confirm it fails.
  int out_int_value;
  ASSERT_FALSE(
      user_reporting_settings_->GetInteger(kSettingPath, &out_int_value));
  const base::Value::List* out_list_value = nullptr;
  ASSERT_FALSE(
      user_reporting_settings_->GetList(kSettingPath, &out_list_value));
  EXPECT_THAT(out_list_value, IsNull());
}

TEST_F(UserReportingSettingsTest, InvalidListPrefPath) {
  profile_->GetTestingPrefService()->registry()->RegisterListPref(
      kSettingPath, /*default_value=*/base::Value::List());

  // Attempt to retrieve the list reporting setting with incorrect data type and
  // confirm it fails.
  bool out_bool_value;
  ASSERT_FALSE(
      user_reporting_settings_->GetBoolean(kSettingPath, &out_bool_value));
  int out_int_value;
  EXPECT_FALSE(
      user_reporting_settings_->GetInteger(kSettingPath, &out_int_value));
}

TEST_F(UserReportingSettingsTest, GetUnregisteredSetting) {
  bool out_bool_value;
  EXPECT_FALSE(
      user_reporting_settings_->GetBoolean(kSettingPath, &out_bool_value));
  int out_int_value;
  EXPECT_FALSE(
      user_reporting_settings_->GetInteger(kSettingPath, &out_int_value));
  const base::Value::List* out_list_value = nullptr;
  EXPECT_FALSE(
      user_reporting_settings_->GetList(kSettingPath, &out_list_value));
}

TEST_F(UserReportingSettingsTest, GetBoolean) {
  profile_->GetTestingPrefService()->registry()->RegisterBooleanPref(
      kSettingPath, /*default_value=*/false);
  bool out_value = true;
  ASSERT_TRUE(user_reporting_settings_->GetBoolean(kSettingPath, &out_value));
  ASSERT_FALSE(out_value);

  // Update setting value and ensure the next fetch returns the updated value.
  profile_->GetPrefs()->SetBoolean(kSettingPath, true);
  ASSERT_TRUE(user_reporting_settings_->GetBoolean(kSettingPath, &out_value));
  EXPECT_TRUE(out_value);
}

TEST_F(UserReportingSettingsTest, GetReportingEnabled_Boolean) {
  profile_->GetTestingPrefService()->registry()->RegisterBooleanPref(
      kSettingPath, /*default_value=*/false);
  bool out_value = true;
  ASSERT_TRUE(
      user_reporting_settings_->GetReportingEnabled(kSettingPath, &out_value));
  EXPECT_FALSE(out_value);

  // Update setting value and ensure the next fetch returns the updated value.
  profile_->GetPrefs()->SetBoolean(kSettingPath, true);
  ASSERT_TRUE(user_reporting_settings_->GetBoolean(kSettingPath, &out_value));
  EXPECT_TRUE(out_value);
}

TEST_F(UserReportingSettingsTest, GetInteger) {
  profile_->GetTestingPrefService()->registry()->RegisterIntegerPref(
      kSettingPath, /*default_value=*/0);
  int out_value;
  ASSERT_TRUE(user_reporting_settings_->GetInteger(kSettingPath, &out_value));
  ASSERT_THAT(out_value, Eq(0));

  // Update setting value and ensure the next fetch returns the updated value.
  static constexpr int new_value = 1;
  profile_->GetPrefs()->SetInteger(kSettingPath, new_value);
  ASSERT_TRUE(user_reporting_settings_->GetInteger(kSettingPath, &out_value));
  EXPECT_THAT(out_value, Eq(new_value));
}

TEST_F(UserReportingSettingsTest, GetList) {
  profile_->GetTestingPrefService()->registry()->RegisterListPref(
      kSettingPath, /*default_value=*/base::Value::List());
  const base::Value::List* out_value;
  ASSERT_TRUE(user_reporting_settings_->GetList(kSettingPath, &out_value));
  ASSERT_THAT(out_value, NotNull());
  EXPECT_TRUE(out_value->empty());

  // Update setting value and ensure the next fetch returns the updated value.
  static constexpr char kListSettingItem[] = "item";
  profile_->GetPrefs()->SetList(kSettingPath,
                                base::Value::List().Append(kListSettingItem));
  ASSERT_TRUE(user_reporting_settings_->GetList(kSettingPath, &out_value));
  ASSERT_THAT(out_value, NotNull());
  ASSERT_THAT(out_value->size(), Eq(1uL));
  EXPECT_THAT(out_value->front().GetString(), Eq(kListSettingItem));
}

TEST_F(UserReportingSettingsTest, GetReportingEnabled_List) {
  profile_->GetTestingPrefService()->registry()->RegisterListPref(
      kSettingPath, /*default_value=*/base::Value::List());
  bool out_value = true;
  ASSERT_TRUE(
      user_reporting_settings_->GetReportingEnabled(kSettingPath, &out_value));
  EXPECT_FALSE(out_value);

  // Update setting value and ensure the next fetch returns the updated value.
  static constexpr char kListSettingItem[] = "item";
  profile_->GetPrefs()->SetList(kSettingPath,
                                base::Value::List().Append(kListSettingItem));
  ASSERT_TRUE(
      user_reporting_settings_->GetReportingEnabled(kSettingPath, &out_value));
  EXPECT_TRUE(out_value);
}

TEST_F(UserReportingSettingsTest, ObserveBooleanSetting) {
  profile_->GetTestingPrefService()->registry()->RegisterBooleanPref(
      kSettingPath, /*default_value=*/false);
  bool callback_called = false;
  const auto callback_subscription =
      user_reporting_settings_->AddSettingsObserver(
          kSettingPath, base::BindLambdaForTesting(
                            [&callback_called]() { callback_called = true; }));

  // Update setting value and ensure callback was triggered.
  profile_->GetPrefs()->SetBoolean(kSettingPath, true);
  ASSERT_TRUE(callback_called);
}

TEST_F(UserReportingSettingsTest, ObserveIntegerSetting) {
  profile_->GetTestingPrefService()->registry()->RegisterIntegerPref(
      kSettingPath, /*default_value=*/0);
  bool callback_called = false;
  const auto callback_subscription =
      user_reporting_settings_->AddSettingsObserver(
          kSettingPath, base::BindLambdaForTesting(
                            [&callback_called]() { callback_called = true; }));

  // Update setting value and ensure callback was triggered.
  profile_->GetPrefs()->SetInteger(kSettingPath, 1);
  ASSERT_TRUE(callback_called);
}

TEST_F(UserReportingSettingsTest, ObserveListSetting) {
  profile_->GetTestingPrefService()->registry()->RegisterListPref(
      kSettingPath, /*default_value=*/base::Value::List());
  bool callback_called = false;
  const auto callback_subscription =
      user_reporting_settings_->AddSettingsObserver(
          kSettingPath, base::BindLambdaForTesting(
                            [&callback_called]() { callback_called = true; }));

  // Update setting value and ensure callback was triggered.
  profile_->GetPrefs()->SetList(kSettingPath,
                                base::Value::List().Append("item"));
  ASSERT_TRUE(callback_called);
}

TEST_F(UserReportingSettingsTest, MultipleSettingObservers) {
  profile_->GetTestingPrefService()->registry()->RegisterBooleanPref(
      kSettingPath, /*default_value=*/false);
  int callback_trigger_count = 0;
  const auto callback_subscription_1 =
      user_reporting_settings_->AddSettingsObserver(
          kSettingPath, base::BindLambdaForTesting([&callback_trigger_count]() {
            callback_trigger_count++;
          }));
  const auto callback_subscription_2 =
      user_reporting_settings_->AddSettingsObserver(
          kSettingPath, base::BindLambdaForTesting([&callback_trigger_count]() {
            callback_trigger_count++;
          }));

  // Update setting value and ensure both callbacks were triggered.
  profile_->GetPrefs()->SetBoolean(kSettingPath, true);
  ASSERT_THAT(callback_trigger_count, Eq(2));
}

TEST_F(UserReportingSettingsTest, OnProfileDestruction) {
  profile_->GetTestingPrefService()->registry()->RegisterBooleanPref(
      kSettingPath, /*default_value=*/false);
  const auto callback_subscription =
      user_reporting_settings_->AddSettingsObserver(kSettingPath,
                                                    base::DoNothing());
  ASSERT_TRUE(
      user_reporting_settings_->IsObservingSettingsForTest(kSettingPath));

  // Delete profile and ensure the setting is no longer observed.
  profile_manager_.DeleteAllTestingProfiles();
  EXPECT_FALSE(
      user_reporting_settings_->IsObservingSettingsForTest(kSettingPath));
}

}  // namespace
}  // namespace reporting

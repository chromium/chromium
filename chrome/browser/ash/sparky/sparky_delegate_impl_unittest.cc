// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sparky/sparky_delegate_impl.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/manta/sparky/sparky_delegate.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
using SettingsPrivatePrefType = extensions::api::settings_private::PrefType;
}  // namespace

class SparkyDelegateImplTest : public testing::Test {
 public:
  SparkyDelegateImplTest() = default;

  SparkyDelegateImplTest(const SparkyDelegateImplTest&) = delete;
  SparkyDelegateImplTest& operator=(const SparkyDelegateImplTest&) = delete;

  ~SparkyDelegateImplTest() override = default;

  SparkyDelegateImpl* GetSparkyDelegateImpl() {
    return sparky_delegate_impl_.get();
  }

  const base::Value& GetPref(const std::string& setting_id) {
    return profile_->GetPrefs()->GetValue(setting_id);
  }

  void SetBool(const std::string& setting_id, bool bool_val) {
    profile_->GetPrefs()->SetBoolean(setting_id, bool_val);
  }

  void AddToMap(const std::string& pref_name,
                SettingsPrivatePrefType settings_pref_type,
                std::optional<base::Value> value) {
    sparky_delegate_impl_->AddPrefToMap(pref_name, settings_pref_type,
                                        std::move(value));
  }

  SparkyDelegateImpl::SettingsDataList* GetCurrentPrefs() {
    return &sparky_delegate_impl_->current_prefs_;
  }

  // testing::Test:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    sparky_delegate_impl_ =
        std::make_unique<SparkyDelegateImpl>(profile_.get());
  }

  void TearDown() override { sparky_delegate_impl_.reset(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<SparkyDelegateImpl> sparky_delegate_impl_;
};

TEST_F(SparkyDelegateImplTest, SetSettings) {
  SetBool(prefs::kDarkModeEnabled, false);
  SetBool(prefs::kPowerAdaptiveChargingEnabled, true);
  ASSERT_TRUE(GetSparkyDelegateImpl()->SetSettings(
      std::make_unique<manta::SettingsData>(prefs::kDarkModeEnabled,
                                            manta::PrefType::kBoolean,
                                            base::Value(true))));
  ASSERT_TRUE(GetSparkyDelegateImpl()->SetSettings(
      std::make_unique<manta::SettingsData>(
          prefs::kPowerAdaptiveChargingEnabled, manta::PrefType::kBoolean,
          base::Value(false))));
  const base::Value& dark_mode_val = GetPref(prefs::kDarkModeEnabled);
  const base::Value& adaptive_charging_val =
      GetPref(prefs::kPowerAdaptiveChargingEnabled);
  RunUntilIdle();
  ASSERT_TRUE(dark_mode_val.is_bool());
  ASSERT_TRUE(dark_mode_val.GetBool());
  ASSERT_TRUE(adaptive_charging_val.is_bool());
  ASSERT_FALSE(adaptive_charging_val.GetBool());
}

TEST_F(SparkyDelegateImplTest, AddPrefToMap) {
  AddToMap("bool pref", SettingsPrivatePrefType::kBoolean,
           std::make_optional<base::Value>(true));
  AddToMap("int pref", SettingsPrivatePrefType::kNumber,
           std::make_optional<base::Value>(1));
  AddToMap("double pref", SettingsPrivatePrefType::kNumber,
           std::make_optional<base::Value>(0.5));
  AddToMap("string pref", SettingsPrivatePrefType::kString,
           std::make_optional<base::Value>("my string"));
  RunUntilIdle();
  ASSERT_TRUE(GetCurrentPrefs()->contains("bool pref"));
  ASSERT_TRUE(GetCurrentPrefs()->contains("int pref"));
  ASSERT_TRUE(GetCurrentPrefs()->contains("double pref"));
  ASSERT_TRUE(GetCurrentPrefs()->contains("string pref"));
  ASSERT_EQ(GetCurrentPrefs()->find("bool pref")->second->pref_name,
            "bool pref");
  ASSERT_EQ(GetCurrentPrefs()->find("int pref")->second->pref_name, "int pref");
  ASSERT_EQ(GetCurrentPrefs()->find("double pref")->second->pref_name,
            "double pref");
  ASSERT_EQ(GetCurrentPrefs()->find("string pref")->second->pref_name,
            "string pref");
  ASSERT_EQ(GetCurrentPrefs()->find("bool pref")->second->pref_type,
            manta::PrefType::kBoolean);
  ASSERT_EQ(GetCurrentPrefs()->find("int pref")->second->pref_type,
            manta::PrefType::kInt);
  ASSERT_EQ(GetCurrentPrefs()->find("double pref")->second->pref_type,
            manta::PrefType::kDouble);
  ASSERT_EQ(GetCurrentPrefs()->find("string pref")->second->pref_type,
            manta::PrefType::kString);
  ASSERT_TRUE(GetCurrentPrefs()->find("bool pref")->second->value->GetBool());
  ASSERT_EQ(GetCurrentPrefs()->find("int pref")->second->value->GetInt(), 1);
  ASSERT_EQ(GetCurrentPrefs()->find("double pref")->second->value->GetDouble(),
            0.5);
  ASSERT_EQ(GetCurrentPrefs()->find("string pref")->second->value->GetString(),
            "my string");
}

}  // namespace ash

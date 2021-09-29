// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/quick_answers_state_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "components/language/core/browser/pref_names.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace ash {

class QuickAnswersStateControllerV1Test : public AshTestBase {
 protected:
  QuickAnswersStateControllerV1Test() {
    scoped_feature_list_.InitAndDisableFeature(
        chromeos::features::kQuickAnswersV2);
  }
  QuickAnswersStateControllerV1Test(const QuickAnswersStateControllerV1Test&) =
      delete;
  QuickAnswersStateControllerV1Test& operator=(
      const QuickAnswersStateControllerV1Test&) = delete;
  ~QuickAnswersStateControllerV1Test() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    prefs_ = Shell::Get()->session_controller()->GetPrimaryUserPrefService();
    DCHECK(prefs_);
  }

  PrefService* prefs() { return prefs_; }

  void NotifyFeatureEligible() {
    prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, true);
    prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantContextEnabled,
                        true);
    AssistantState::Get()->NotifyFeatureAllowed(
        chromeos::assistant::AssistantAllowedState::ALLOWED);
    AssistantState::Get()->NotifyLocaleChanged(ULOC_US);
  }

 private:
  PrefService* prefs_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(QuickAnswersStateControllerV1Test, FeatureEligible) {
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
  NotifyFeatureEligible();
  EXPECT_TRUE(ash::QuickAnswersState::Get()->is_eligible());
}

TEST_F(QuickAnswersStateControllerV1Test,
       FeatureIneligibleWhenAssistantDisabled) {
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
  NotifyFeatureEligible();
  EXPECT_TRUE(ash::QuickAnswersState::Get()->is_eligible());

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, false);
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
}

TEST_F(QuickAnswersStateControllerV1Test,
       FeatureIneligibleWhenAssistantContextDisabled) {
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
  NotifyFeatureEligible();
  EXPECT_TRUE(ash::QuickAnswersState::Get()->is_eligible());

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantContextEnabled,
                      false);
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
}

TEST_F(QuickAnswersStateControllerV1Test,
       FeatureIneligibleWhenAssistantNotAllowed) {
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
  NotifyFeatureEligible();
  EXPECT_TRUE(ash::QuickAnswersState::Get()->is_eligible());

  AssistantState::Get()->NotifyFeatureAllowed(
      chromeos::assistant::AssistantAllowedState::DISALLOWED_BY_POLICY);
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
}

TEST_F(QuickAnswersStateControllerV1Test,
       FeatureIneligibleWhenLocaleNotSupported) {
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
  NotifyFeatureEligible();
  EXPECT_TRUE(ash::QuickAnswersState::Get()->is_eligible());

  UErrorCode error_code = U_ZERO_ERROR;
  const icu::Locale& old_locale = icu::Locale::getDefault();
  icu::Locale::setDefault(icu::Locale(ULOC_JAPAN), error_code);
  AssistantState::Get()->NotifyLocaleChanged(ULOC_JAPAN);

  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
  icu::Locale::setDefault(old_locale, error_code);
}

class TestQuickAnswersStateObserver : public QuickAnswersStateObserver {
 public:
  TestQuickAnswersStateObserver() = default;

  TestQuickAnswersStateObserver(const TestQuickAnswersStateObserver&) = delete;
  TestQuickAnswersStateObserver& operator=(
      const TestQuickAnswersStateObserver&) = delete;

  ~TestQuickAnswersStateObserver() override = default;

  // QuickAnswersStateObserver:
  void OnSettingsEnabled(bool settings_enabled) override {
    settings_enabled_ = settings_enabled;
  }

  bool settings_enabled() const { return settings_enabled_; }

 private:
  bool settings_enabled_ = false;
};

class QuickAnswersStateControllerTest : public AshTestBase {
 protected:
  QuickAnswersStateControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kQuickAnswersV2);
  }
  QuickAnswersStateControllerTest(const QuickAnswersStateControllerTest&) =
      delete;
  QuickAnswersStateControllerTest& operator=(
      const QuickAnswersStateControllerTest&) = delete;
  ~QuickAnswersStateControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    prefs_ = Shell::Get()->session_controller()->GetPrimaryUserPrefService();
    DCHECK(prefs_);

    observer_ = std::make_unique<TestQuickAnswersStateObserver>();
  }

  PrefService* prefs() { return prefs_; }

  TestQuickAnswersStateObserver* observer() { return observer_.get(); }

 private:
  PrefService* prefs_ = nullptr;
  std::unique_ptr<TestQuickAnswersStateObserver> observer_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(QuickAnswersStateControllerTest, InitObserver) {
  EXPECT_FALSE(ash::QuickAnswersState::Get()->settings_enabled());
  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersEnabled, true);

  // The observer class should get an instant notification about the current
  // pref value.
  QuickAnswersState::Get()->AddObserver(observer());
  EXPECT_TRUE(ash::QuickAnswersState::Get()->settings_enabled());
  EXPECT_TRUE(observer()->settings_enabled());

  QuickAnswersState::Get()->RemoveObserver(observer());
}

TEST_F(QuickAnswersStateControllerTest, NotifySettingsEnabled) {
  QuickAnswersState::Get()->AddObserver(observer());

  EXPECT_FALSE(ash::QuickAnswersState::Get()->settings_enabled());
  EXPECT_FALSE(observer()->settings_enabled());

  // The observer class should get an notification when the pref value changes.
  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersEnabled, true);
  EXPECT_TRUE(ash::QuickAnswersState::Get()->settings_enabled());
  EXPECT_TRUE(observer()->settings_enabled());

  QuickAnswersState::Get()->RemoveObserver(observer());
}

TEST_F(QuickAnswersStateControllerTest, Eligibility) {
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale(ULOC_US), error_code);
  prefs()->SetString(language::prefs::kApplicationLocale, "en");
  EXPECT_TRUE(ash::QuickAnswersState::Get()->is_eligible());

  icu::Locale::setDefault(icu::Locale(ULOC_CHINESE), error_code);
  prefs()->SetString(language::prefs::kApplicationLocale, "zh");
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());

  icu::Locale::setDefault(icu::Locale(ULOC_US), error_code);
  prefs()->SetString(language::prefs::kApplicationLocale, "en");
}

}  // namespace ash

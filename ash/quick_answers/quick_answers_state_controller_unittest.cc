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
#include "third_party/icu/source/common/unicode/locid.h"

namespace ash {

class QuickAnswersStateControllerTest : public AshTestBase {
 protected:
  QuickAnswersStateControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kQuickAnswers);
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

TEST_F(QuickAnswersStateControllerTest, FeatureEligible) {
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
  NotifyFeatureEligible();
  EXPECT_TRUE(ash::QuickAnswersState::Get()->is_eligible());
}

TEST_F(QuickAnswersStateControllerTest,
       FeatureIneligibleWhenAssistantDisabled) {
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
  NotifyFeatureEligible();
  EXPECT_TRUE(ash::QuickAnswersState::Get()->is_eligible());

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, false);
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
}

TEST_F(QuickAnswersStateControllerTest,
       FeatureIneligibleWhenAssistantContextDisabled) {
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
  NotifyFeatureEligible();
  EXPECT_TRUE(ash::QuickAnswersState::Get()->is_eligible());

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantContextEnabled,
                      false);
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
}

TEST_F(QuickAnswersStateControllerTest,
       FeatureIneligibleWhenAssistantNotAllowed) {
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
  NotifyFeatureEligible();
  EXPECT_TRUE(ash::QuickAnswersState::Get()->is_eligible());

  AssistantState::Get()->NotifyFeatureAllowed(
      chromeos::assistant::AssistantAllowedState::DISALLOWED_BY_POLICY);
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
}

TEST_F(QuickAnswersStateControllerTest,
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

}  // namespace ash

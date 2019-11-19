// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_state_controller.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "components/prefs/pref_service.h"

namespace ash {

class TestAssistantStateObserver : public AssistantStateObserver {
 public:
  TestAssistantStateObserver() = default;
  ~TestAssistantStateObserver() override = default;

  // AssistantStateObserver:
  void OnAssistantConsentStatusChanged(int consent_status) override {
    consent_status_ = consent_status;
  }
  void OnAssistantContextEnabled(bool context_enabled) override {
    context_enabled_ = context_enabled;
  }
  void OnAssistantSettingsEnabled(bool settings_enabled) override {
    settings_enabled_ = settings_enabled;
  }
  void OnAssistantHotwordAlwaysOn(bool hotword_always_on) override {
    hotword_always_on_ = hotword_always_on;
  }
  void OnAssistantHotwordEnabled(bool hotword_enabled) override {
    hotword_enabled_ = hotword_enabled;
  }
  void OnAssistantLaunchWithMicOpen(bool launch_with_mic_open) override {
    launch_with_mic_open_ = launch_with_mic_open;
  }
  void OnAssistantNotificationEnabled(bool notification_enabled) override {
    notification_enabled_ = notification_enabled;
  }

  int consent_status() const { return consent_status_; }
  bool context_enabled() const { return context_enabled_; }
  bool settings_enabled() const { return settings_enabled_; }
  bool hotword_always_on() const { return hotword_always_on_; }
  bool hotword_enabled() const { return hotword_enabled_; }
  bool launch_with_mic_open() const { return launch_with_mic_open_; }
  bool notification_enabled() const { return notification_enabled_; }

 private:
  int consent_status_ = chromeos::assistant::prefs::ConsentStatus::kUnknown;
  bool context_enabled_ = false;
  bool settings_enabled_ = false;
  bool hotword_always_on_ = false;
  bool hotword_enabled_ = false;
  bool launch_with_mic_open_ = false;
  bool notification_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestAssistantStateObserver);
};

class AssistantStateControllerTest : public AshTestBase {
 protected:
  AssistantStateControllerTest() = default;
  ~AssistantStateControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    prefs_ = Shell::Get()->session_controller()->GetPrimaryUserPrefService();
    DCHECK(prefs_);

    observer_ = std::make_unique<TestAssistantStateObserver>();
  }

  PrefService* prefs() { return prefs_; }

  TestAssistantStateObserver* observer() { return observer_.get(); }

 private:
  PrefService* prefs_ = nullptr;
  std::unique_ptr<TestAssistantStateObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(AssistantStateControllerTest);
};

TEST_F(AssistantStateControllerTest, InitObserver) {
  prefs()->SetInteger(
      chromeos::assistant::prefs::kAssistantConsentStatus,
      chromeos::assistant::prefs::ConsentStatus::kActivityControlAccepted);
  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantContextEnabled,
                      true);
  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, true);
  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantHotwordAlwaysOn,
                      true);
  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantHotwordEnabled,
                      true);
  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantLaunchWithMicOpen,
                      true);
  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantNotificationEnabled,
                      true);

  // The observer class should get an instant notification about the current
  // pref value.
  AssistantState::Get()->AddObserver(observer());
  EXPECT_EQ(chromeos::assistant::prefs::ConsentStatus::kActivityControlAccepted,
            observer()->consent_status());
  EXPECT_EQ(observer()->context_enabled(), true);
  EXPECT_EQ(observer()->settings_enabled(), true);
  EXPECT_EQ(observer()->hotword_always_on(), true);
  EXPECT_EQ(observer()->hotword_enabled(), true);
  EXPECT_EQ(observer()->launch_with_mic_open(), true);
  EXPECT_EQ(observer()->notification_enabled(), true);
  AssistantState::Get()->RemoveObserver(observer());
}

TEST_F(AssistantStateControllerTest, NotifyConsentStatus) {
  AssistantState::Get()->AddObserver(observer());

  prefs()->SetInteger(chromeos::assistant::prefs::kAssistantConsentStatus,
                      chromeos::assistant::prefs::ConsentStatus::kUnauthorized);
  EXPECT_EQ(chromeos::assistant::prefs::ConsentStatus::kUnauthorized,
            observer()->consent_status());

  prefs()->SetInteger(
      chromeos::assistant::prefs::kAssistantConsentStatus,
      chromeos::assistant::prefs::ConsentStatus::kActivityControlAccepted);
  EXPECT_EQ(chromeos::assistant::prefs::ConsentStatus::kActivityControlAccepted,
            observer()->consent_status());
  AssistantState::Get()->RemoveObserver(observer());
}

TEST_F(AssistantStateControllerTest, NotifyContextEnabled) {
  AssistantState::Get()->AddObserver(observer());

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantContextEnabled,
                      false);
  EXPECT_EQ(observer()->context_enabled(), false);

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantContextEnabled,
                      true);
  EXPECT_EQ(observer()->context_enabled(), true);
  AssistantState::Get()->RemoveObserver(observer());
}

TEST_F(AssistantStateControllerTest, NotifySettingsEnabled) {
  AssistantState::Get()->AddObserver(observer());

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, false);
  EXPECT_EQ(observer()->settings_enabled(), false);

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, true);
  EXPECT_EQ(observer()->settings_enabled(), true);
  AssistantState::Get()->RemoveObserver(observer());
}

TEST_F(AssistantStateControllerTest, NotifyHotwordAlwaysOn) {
  AssistantState::Get()->AddObserver(observer());

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantHotwordAlwaysOn,
                      false);
  EXPECT_EQ(observer()->hotword_always_on(), false);

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantHotwordAlwaysOn,
                      true);
  EXPECT_EQ(observer()->hotword_always_on(), true);
  AssistantState::Get()->RemoveObserver(observer());
}

TEST_F(AssistantStateControllerTest, NotifyHotwordEnabled) {
  AssistantState::Get()->AddObserver(observer());

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantHotwordEnabled,
                      false);
  EXPECT_EQ(observer()->hotword_enabled(), false);

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantHotwordEnabled,
                      true);
  EXPECT_EQ(observer()->hotword_enabled(), true);
  AssistantState::Get()->RemoveObserver(observer());
}

TEST_F(AssistantStateControllerTest, NotifyLaunchWithMicOpen) {
  AssistantState::Get()->AddObserver(observer());

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantLaunchWithMicOpen,
                      false);
  EXPECT_EQ(observer()->launch_with_mic_open(), false);

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantLaunchWithMicOpen,
                      true);
  EXPECT_EQ(observer()->launch_with_mic_open(), true);
  AssistantState::Get()->RemoveObserver(observer());
}

TEST_F(AssistantStateControllerTest, NotifyNotificationEnabled) {
  AssistantState::Get()->AddObserver(observer());

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantNotificationEnabled,
                      false);
  EXPECT_EQ(observer()->notification_enabled(), false);

  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantNotificationEnabled,
                      true);
  EXPECT_EQ(observer()->notification_enabled(), true);
  AssistantState::Get()->RemoveObserver(observer());
}

}  // namespace ash

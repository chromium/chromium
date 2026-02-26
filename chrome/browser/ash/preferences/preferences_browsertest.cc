// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <sys/types.h>

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/input_method/input_method_manager_impl.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system/fake_input_device_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/events/event_utils.h"

namespace ash {

class PreferencesTest : public LoginManagerTest {
 public:
  PreferencesTest()
      : LoginManagerTest(), input_settings_(nullptr), keyboard_(nullptr) {
    login_mixin_.AppendRegularUsers(2);
  }

  PreferencesTest(const PreferencesTest&) = delete;
  PreferencesTest& operator=(const PreferencesTest&) = delete;

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    input_settings_ = system::InputDeviceSettings::Get()->GetFakeInterface();
    EXPECT_NE(nullptr, input_settings_);

    user_manager::UserManager::Get()->SetOwnerId(
        login_mixin_.users()[0].account_id);

    // In browser_test environment, FakeImeKeyboard is used by
    // InputMethodManager.
    keyboard_ = static_cast<input_method::FakeImeKeyboard*>(
        input_method::InputMethodManager::Get()->GetImeKeyboard());
  }

  // Sets set of preferences in given |prefs|. Value of preference depends on
  // |variant| value. For opposite |variant| values all preferences receive
  // different values.
  void SetPrefs(PrefService* prefs, bool variant) {
    prefs->SetBoolean(ash::prefs::kMouseReverseScroll, variant);
    prefs->SetBoolean(ash::prefs::kNaturalScroll, variant);
    prefs->SetBoolean(ash::prefs::kTapToClickEnabled, variant);
    prefs->SetBoolean(ash::prefs::kPrimaryMouseButtonRight, !variant);
    prefs->SetBoolean(ash::prefs::kPrimaryPointingStickButtonRight, !variant);
    prefs->SetBoolean(ash::prefs::kMouseAcceleration, variant);
    prefs->SetBoolean(ash::prefs::kMouseScrollAcceleration, variant);
    prefs->SetBoolean(ash::prefs::kPointingStickAcceleration, variant);
    prefs->SetBoolean(ash::prefs::kTouchpadAcceleration, variant);
    prefs->SetBoolean(ash::prefs::kTouchpadScrollAcceleration, variant);
    prefs->SetBoolean(ash::prefs::kTouchpadHapticFeedback, variant);
    prefs->SetBoolean(ash::prefs::kEnableTouchpadThreeFingerClick, !variant);
    prefs->SetInteger(ash::prefs::kMouseSensitivity, !variant);
    prefs->SetInteger(ash::prefs::kMouseScrollSensitivity, variant ? 1 : 4);
    prefs->SetInteger(ash::prefs::kPointingStickSensitivity, !variant);
    prefs->SetInteger(ash::prefs::kTouchpadSensitivity, variant);
    prefs->SetInteger(ash::prefs::kTouchpadHapticClickSensitivity,
                      variant ? 1 : 3);
    prefs->SetInteger(ash::prefs::kTouchpadScrollSensitivity, variant ? 1 : 4);
    prefs->SetBoolean(ash::prefs::kXkbAutoRepeatEnabled, variant);
    prefs->SetInteger(ash::prefs::kXkbAutoRepeatDelay, variant ? 100 : 500);
    prefs->SetInteger(ash::prefs::kXkbAutoRepeatInterval, variant ? 1 : 4);
    prefs->SetString(
        ash::prefs::kLanguagePreloadEngines,
        variant ? "xkb:us::eng,xkb:us:dvorak:eng" : "xkb:us::eng,xkb:ru::rus");
  }

  void CheckSettingsCorrespondToPrefs(PrefService* prefs) {
    EXPECT_EQ(prefs->GetBoolean(ash::prefs::kTapToClickEnabled),
              input_settings_->current_touchpad_settings().GetTapToClick());
    EXPECT_EQ(
        prefs->GetBoolean(ash::prefs::kPrimaryMouseButtonRight),
        input_settings_->current_mouse_settings().GetPrimaryButtonRight());
    EXPECT_EQ(prefs->GetBoolean(ash::prefs::kPrimaryPointingStickButtonRight),
              input_settings_->current_pointing_stick_settings()
                  .GetPrimaryButtonRight());
    EXPECT_EQ(prefs->GetBoolean(ash::prefs::kMouseReverseScroll),
              input_settings_->current_mouse_settings().GetReverseScroll());
    EXPECT_EQ(prefs->GetBoolean(ash::prefs::kMouseAcceleration),
              input_settings_->current_mouse_settings().GetAcceleration());
    EXPECT_EQ(
        prefs->GetBoolean(ash::prefs::kMouseScrollAcceleration),
        input_settings_->current_mouse_settings().GetScrollAcceleration());
    EXPECT_EQ(
        prefs->GetBoolean(ash::prefs::kPointingStickAcceleration),
        input_settings_->current_pointing_stick_settings().GetAcceleration());
    EXPECT_EQ(prefs->GetBoolean(ash::prefs::kTouchpadAcceleration),
              input_settings_->current_touchpad_settings().GetAcceleration());
    EXPECT_EQ(
        prefs->GetBoolean(ash::prefs::kTouchpadScrollAcceleration),
        input_settings_->current_touchpad_settings().GetScrollAcceleration());
    EXPECT_EQ(prefs->GetBoolean(ash::prefs::kTouchpadHapticFeedback),
              input_settings_->current_touchpad_settings().GetHapticFeedback());
    EXPECT_EQ(
        prefs->GetBoolean(ash::prefs::kEnableTouchpadThreeFingerClick),
        input_settings_->current_touchpad_settings().GetThreeFingerClick());
    EXPECT_EQ(prefs->GetInteger(ash::prefs::kMouseSensitivity),
              input_settings_->current_mouse_settings().GetSensitivity());
    EXPECT_EQ(prefs->GetInteger(ash::prefs::kMouseScrollSensitivity),
              input_settings_->current_mouse_settings().GetScrollSensitivity());
    EXPECT_EQ(
        prefs->GetInteger(ash::prefs::kPointingStickSensitivity),
        input_settings_->current_pointing_stick_settings().GetSensitivity());
    EXPECT_EQ(prefs->GetInteger(ash::prefs::kTouchpadSensitivity),
              input_settings_->current_touchpad_settings().GetSensitivity());
    EXPECT_EQ(prefs->GetInteger(ash::prefs::kTouchpadHapticClickSensitivity),
              input_settings_->current_touchpad_settings()
                  .GetHapticClickSensitivity());
    EXPECT_EQ(
        prefs->GetInteger(ash::prefs::kTouchpadScrollSensitivity),
        input_settings_->current_touchpad_settings().GetScrollSensitivity());
    EXPECT_EQ(prefs->GetBoolean(ash::prefs::kXkbAutoRepeatEnabled),
              keyboard_->GetAutoRepeatEnabled());
    input_method::AutoRepeatRate rate = keyboard_->last_auto_repeat_rate_;
    EXPECT_EQ(prefs->GetInteger(ash::prefs::kXkbAutoRepeatDelay),
              rate.initial_delay.InMilliseconds());
    EXPECT_EQ(prefs->GetInteger(ash::prefs::kXkbAutoRepeatInterval),
              rate.repeat_interval.InMilliseconds());
    EXPECT_EQ(prefs->GetString(ash::prefs::kLanguageCurrentInputMethod),
              input_method::InputMethodManager::Get()
                  ->GetActiveIMEState()
                  ->GetCurrentInputMethod()
                  .id());
  }

  void CheckLocalStateCorrespondsToPrefs(PrefService* prefs) {
    PrefService* local_state = g_browser_process->local_state();
    EXPECT_EQ(local_state->GetBoolean(ash::prefs::kOwnerTapToClickEnabled),
              prefs->GetBoolean(ash::prefs::kTapToClickEnabled));
    EXPECT_EQ(
        local_state->GetBoolean(ash::prefs::kOwnerPrimaryMouseButtonRight),
        prefs->GetBoolean(ash::prefs::kPrimaryMouseButtonRight));
    EXPECT_EQ(local_state->GetBoolean(
                  ash::prefs::kOwnerPrimaryPointingStickButtonRight),
              prefs->GetBoolean(ash::prefs::kPrimaryPointingStickButtonRight));
  }

  LoginManagerMixin login_mixin_{&mixin_host_};

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<system::InputDeviceSettings::FakeInterface> input_settings_;
  raw_ptr<input_method::FakeImeKeyboard, DanglingUntriaged> keyboard_;
};

IN_PROC_BROWSER_TEST_F(PreferencesTest, MultiProfiles) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const auto& users = login_mixin_.users();
  // Add first user and init its preferences. Check that corresponding
  // settings has been changed.
  LoginUser(users[0].account_id);
  const user_manager::User* user1 = user_manager->FindUser(users[0].account_id);
  PrefService* prefs1 =
      ProfileHelper::Get()->GetProfileByUser(user1)->GetPrefs();
  SetPrefs(prefs1, false);
  content::RunAllPendingInMessageLoop();
  CheckSettingsCorrespondToPrefs(prefs1);

  // Add second user and init its prefs with different values.
  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(users[1].account_id);
  content::RunAllPendingInMessageLoop();
  const user_manager::User* user2 = user_manager->FindUser(users[1].account_id);
  EXPECT_TRUE(user2->is_active());
  PrefService* prefs2 =
      ProfileHelper::Get()->GetProfileByUser(user2)->GetPrefs();
  SetPrefs(prefs2, true);

  // Check that settings were changed accordingly.
  EXPECT_TRUE(user2->is_active());
  CheckSettingsCorrespondToPrefs(prefs2);

  // Check that changing prefs of the active user doesn't affect prefs of the
  // inactive user.
  base::DictValue prefs_backup =
      prefs1->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS);
  SetPrefs(prefs2, false);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_EQ(prefs_backup,
            prefs1->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS));
  SetPrefs(prefs2, true);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_EQ(prefs_backup,
            prefs1->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS));

  // Check that changing prefs of the inactive user doesn't affect prefs of the
  // active user.
  prefs_backup = prefs2->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS);
  SetPrefs(prefs1, true);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_EQ(prefs_backup,
            prefs2->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS));
  SetPrefs(prefs1, false);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_EQ(prefs_backup,
            prefs2->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS));

  // Check that changing non-owner prefs doesn't change corresponding local
  // state prefs and vice versa.
  EXPECT_EQ(user_manager->GetOwnerAccountId(), users[0].account_id);
  CheckLocalStateCorrespondsToPrefs(prefs1);
  prefs2->SetBoolean(ash::prefs::kTapToClickEnabled,
                     !prefs1->GetBoolean(ash::prefs::kTapToClickEnabled));
  CheckLocalStateCorrespondsToPrefs(prefs1);
  prefs1->SetBoolean(ash::prefs::kTapToClickEnabled,
                     !prefs1->GetBoolean(ash::prefs::kTapToClickEnabled));
  CheckLocalStateCorrespondsToPrefs(prefs1);

  // Switch user back.
  user_manager->SwitchActiveUser(users[0].account_id);
  CheckSettingsCorrespondToPrefs(prefs1);
  CheckLocalStateCorrespondsToPrefs(prefs1);
}

}  // namespace ash

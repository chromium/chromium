// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <sys/types.h>

#include "ash/public/cpp/ash_switches.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_manager_impl.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/system/fake_input_device_settings.h"
#include "chrome/common/pref_names.h"
#include "components/feedback/tracing_manager.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/fake_ime_keyboard.h"
#include "ui/events/event_utils.h"

namespace chromeos {

class PreferencesTest : public LoginManagerTest {
 public:
  PreferencesTest()
      : LoginManagerTest(true, true),
        input_settings_(nullptr),
        keyboard_(nullptr) {
    struct {
      const char* email;
      const char* gaia_id;
    } const kTestUsers[] = {{"test-user1@gmail.com", "1111111111"},
                            {"test-user2@gmail.com", "2222222222"}};
    for (size_t i = 0; i < base::size(kTestUsers); ++i) {
      test_users_.push_back(AccountId::FromUserEmailGaiaId(
          kTestUsers[i].email, kTestUsers[i].gaia_id));
    }

    scoped_testing_cros_settings_.device_settings()->Set(
        kDeviceOwner, base::Value(test_users_[0].GetUserEmail()));
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    input_settings_ = system::InputDeviceSettings::Get()->GetFakeInterface();
    EXPECT_NE(nullptr, input_settings_);
    keyboard_ = new input_method::FakeImeKeyboard();
    static_cast<input_method::InputMethodManagerImpl*>(
        input_method::InputMethodManager::Get())
        ->SetImeKeyboardForTesting(keyboard_);
  }

  // Sets set of preferences in given |prefs|. Value of prefernece depends of
  // |variant| value. For opposite |variant| values all preferences receive
  // different values.
  void SetPrefs(PrefService* prefs, bool variant) {
    prefs->SetBoolean(prefs::kTapToClickEnabled, variant);
    prefs->SetBoolean(prefs::kPrimaryMouseButtonRight, !variant);
    prefs->SetBoolean(prefs::kMouseReverseScroll, variant);
    prefs->SetBoolean(prefs::kMouseAcceleration, variant);
    prefs->SetBoolean(prefs::kTouchpadAcceleration, variant);
    prefs->SetBoolean(prefs::kEnableTouchpadThreeFingerClick, !variant);
    prefs->SetBoolean(prefs::kNaturalScroll, variant);
    prefs->SetInteger(prefs::kMouseSensitivity, !variant);
    prefs->SetInteger(prefs::kTouchpadSensitivity, variant);
    prefs->SetBoolean(prefs::kLanguageXkbAutoRepeatEnabled, variant);
    prefs->SetInteger(prefs::kLanguageXkbAutoRepeatDelay, variant ? 100 : 500);
    prefs->SetInteger(prefs::kLanguageXkbAutoRepeatInterval, variant ? 1 : 4);
    prefs->SetString(prefs::kLanguagePreloadEngines,
                     variant ? "xkb:us::eng,xkb:us:dvorak:eng"
                             : "xkb:us::eng,xkb:ru::rus");
  }

  void CheckSettingsCorrespondToPrefs(PrefService* prefs) {
    EXPECT_EQ(prefs->GetBoolean(prefs::kTapToClickEnabled),
              input_settings_->current_touchpad_settings().GetTapToClick());
    EXPECT_EQ(prefs->GetBoolean(prefs::kPrimaryMouseButtonRight),
              input_settings_->current_mouse_settings()
                  .GetPrimaryButtonRight());
    EXPECT_EQ(prefs->GetBoolean(prefs::kMouseReverseScroll),
              input_settings_->current_mouse_settings().GetReverseScroll());
    EXPECT_EQ(prefs->GetBoolean(prefs::kMouseAcceleration),
              input_settings_->current_mouse_settings().GetAcceleration());
    EXPECT_EQ(prefs->GetBoolean(prefs::kTouchpadAcceleration),
              input_settings_->current_touchpad_settings().GetAcceleration());
    EXPECT_EQ(prefs->GetBoolean(prefs::kEnableTouchpadThreeFingerClick),
              input_settings_->current_touchpad_settings()
                  .GetThreeFingerClick());
    EXPECT_EQ(prefs->GetInteger(prefs::kMouseSensitivity),
              input_settings_->current_mouse_settings().GetSensitivity());
    EXPECT_EQ(prefs->GetInteger(prefs::kTouchpadSensitivity),
              input_settings_->current_touchpad_settings().GetSensitivity());
    EXPECT_EQ(prefs->GetBoolean(prefs::kLanguageXkbAutoRepeatEnabled),
              keyboard_->auto_repeat_is_enabled_);
    input_method::AutoRepeatRate rate = keyboard_->last_auto_repeat_rate_;
    EXPECT_EQ(prefs->GetInteger(prefs::kLanguageXkbAutoRepeatDelay),
              (int)rate.initial_delay_in_ms);
    EXPECT_EQ(prefs->GetInteger(prefs::kLanguageXkbAutoRepeatInterval),
              (int)rate.repeat_interval_in_ms);
    EXPECT_EQ(prefs->GetString(prefs::kLanguageCurrentInputMethod),
              input_method::InputMethodManager::Get()
                  ->GetActiveIMEState()
                  ->GetCurrentInputMethod()
                  .id());
  }

  void CheckLocalStateCorrespondsToPrefs(PrefService* prefs) {
    PrefService* local_state = g_browser_process->local_state();
    EXPECT_EQ(local_state->GetBoolean(prefs::kOwnerTapToClickEnabled),
              prefs->GetBoolean(prefs::kTapToClickEnabled));
    EXPECT_EQ(local_state->GetBoolean(prefs::kOwnerPrimaryMouseButtonRight),
              prefs->GetBoolean(prefs::kPrimaryMouseButtonRight));
  }

  std::vector<AccountId> test_users_;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;

 private:
  system::InputDeviceSettings::FakeInterface* input_settings_;
  input_method::FakeImeKeyboard* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(PreferencesTest);
};

class PreferencesTestForceWebUiLogin : public PreferencesTest {
 public:
  PreferencesTestForceWebUiLogin() = default;
  ~PreferencesTestForceWebUiLogin() override = default;

  // PreferencesTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PreferencesTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kShowWebUiLogin);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PreferencesTestForceWebUiLogin);
};

IN_PROC_BROWSER_TEST_F(PreferencesTestForceWebUiLogin, PRE_MultiProfiles) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(PreferencesTestForceWebUiLogin, MultiProfiles) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Add first user and init its preferences. Check that corresponding
  // settings has been changed.
  LoginUser(test_users_[0]);
  const user_manager::User* user1 = user_manager->FindUser(test_users_[0]);
  PrefService* prefs1 =
      ProfileHelper::Get()->GetProfileByUserUnsafe(user1)->GetPrefs();
  SetPrefs(prefs1, false);
  content::RunAllPendingInMessageLoop();
  CheckSettingsCorrespondToPrefs(prefs1);

  // Add second user and init its prefs with different values.
  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(test_users_[1]);
  content::RunAllPendingInMessageLoop();
  const user_manager::User* user2 = user_manager->FindUser(test_users_[1]);
  EXPECT_TRUE(user2->is_active());
  PrefService* prefs2 =
      ProfileHelper::Get()->GetProfileByUserUnsafe(user2)->GetPrefs();
  SetPrefs(prefs2, true);

  // Check that settings were changed accordingly.
  EXPECT_TRUE(user2->is_active());
  CheckSettingsCorrespondToPrefs(prefs2);

  // Check that changing prefs of the active user doesn't affect prefs of the
  // inactive user.
  std::unique_ptr<base::DictionaryValue> prefs_backup =
      prefs1->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS);
  SetPrefs(prefs2, false);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_TRUE(prefs_backup->Equals(
      prefs1->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS).get()));
  SetPrefs(prefs2, true);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_TRUE(prefs_backup->Equals(
      prefs1->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS).get()));

  // Check that changing prefs of the inactive user doesn't affect prefs of the
  // active user.
  prefs_backup = prefs2->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS);
  SetPrefs(prefs1, true);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_TRUE(prefs_backup->Equals(
      prefs2->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS).get()));
  SetPrefs(prefs1, false);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_TRUE(prefs_backup->Equals(
      prefs2->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS).get()));

  // Check that changing non-owner prefs doesn't change corresponding local
  // state prefs and vice versa.
  EXPECT_EQ(user_manager->GetOwnerAccountId(), test_users_[0]);
  CheckLocalStateCorrespondsToPrefs(prefs1);
  prefs2->SetBoolean(prefs::kTapToClickEnabled,
                     !prefs1->GetBoolean(prefs::kTapToClickEnabled));
  CheckLocalStateCorrespondsToPrefs(prefs1);
  prefs1->SetBoolean(prefs::kTapToClickEnabled,
                     !prefs1->GetBoolean(prefs::kTapToClickEnabled));
  CheckLocalStateCorrespondsToPrefs(prefs1);

  // Switch user back.
  user_manager->SwitchActiveUser(test_users_[0]);
  CheckSettingsCorrespondToPrefs(prefs1);
  CheckLocalStateCorrespondsToPrefs(prefs1);
}

}  // namespace chromeos

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_persistence.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager_impl.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

namespace {
const char kInputId1[] = "xkb:us:dvorak:eng";
const char kInputId2[] = "xkb:us:colemak:eng";
}  // namespace

class InputMethodPersistenceTest : public testing::Test {
 protected:
  InputMethodPersistenceTest()
      : mock_profile_manager_(TestingBrowserProcess::GetGlobal()),
        fake_user_manager_(new chromeos::FakeChromeUserManager()),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_)) {}

  void SetUp() override {
    ASSERT_TRUE(mock_profile_manager_.SetUp());

    // Add a user.
    const AccountId test_account_id(
        AccountId::FromUserEmail("test-user@example.com"));
    fake_user_manager_->AddUser(test_account_id);
    fake_user_manager_->LoginUser(test_account_id);

    // Create a valid profile for the user.
    TestingProfile* mock_profile = mock_profile_manager_.CreateTestingProfile(
        test_account_id.GetUserEmail());
    EXPECT_EQ(ProfileManager::GetActiveUserProfile(), mock_profile);

    mock_user_prefs_ = mock_profile->GetTestingPrefService();
  }

  // Verifies that the user and system prefs contain the expected values.
  void VerifyPrefs(const std::string& current_input_method,
                   const std::string& previous_input_method,
                   const std::string& preferred_keyboard_layout) {
    EXPECT_EQ(current_input_method,
              mock_user_prefs_->GetString(prefs::kLanguageCurrentInputMethod));
    EXPECT_EQ(previous_input_method,
              mock_user_prefs_->GetString(prefs::kLanguagePreviousInputMethod));
    EXPECT_EQ(preferred_keyboard_layout,
              g_browser_process->local_state()->GetString(
                  language_prefs::kPreferredKeyboardLayout));
  }

  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable* mock_user_prefs_;
  MockInputMethodManagerImpl mock_manager_;
  TestingProfileManager mock_profile_manager_;
  chromeos::FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
};

TEST_F(InputMethodPersistenceTest, TestLifetime) {
  {
    InputMethodPersistence persistence(&mock_manager_);
    EXPECT_EQ(1, mock_manager_.add_observer_count());
  }
  EXPECT_EQ(1, mock_manager_.remove_observer_count());
}

TEST_F(InputMethodPersistenceTest, TestPrefPersistenceByState) {
  InputMethodPersistence persistence(&mock_manager_);

  persistence.OnSessionStateChange(InputMethodManager::STATE_LOGIN_SCREEN);
  mock_manager_.SetCurrentInputMethodId(kInputId1);
  persistence.InputMethodChanged(&mock_manager_,
                                 ProfileManager::GetActiveUserProfile(), false);
  VerifyPrefs("", "", kInputId1);

  persistence.OnSessionStateChange(InputMethodManager::STATE_BROWSER_SCREEN);
  mock_manager_.SetCurrentInputMethodId(kInputId2);
  persistence.InputMethodChanged(&mock_manager_,
                                 ProfileManager::GetActiveUserProfile(), false);
  VerifyPrefs(kInputId2, "", kInputId1);

  persistence.OnSessionStateChange(InputMethodManager::STATE_LOCK_SCREEN);
  mock_manager_.SetCurrentInputMethodId(kInputId1);
  persistence.InputMethodChanged(&mock_manager_,
                                 ProfileManager::GetActiveUserProfile(), false);
  VerifyPrefs(kInputId2, "", kInputId1);

  persistence.OnSessionStateChange(InputMethodManager::STATE_TERMINATING);
  mock_manager_.SetCurrentInputMethodId(kInputId1);
  persistence.InputMethodChanged(&mock_manager_,
                                 ProfileManager::GetActiveUserProfile(), false);
  VerifyPrefs(kInputId2, "", kInputId1);

  persistence.OnSessionStateChange(InputMethodManager::STATE_LOGIN_SCREEN);
  mock_manager_.SetCurrentInputMethodId(kInputId2);
  persistence.InputMethodChanged(&mock_manager_,
                                 ProfileManager::GetActiveUserProfile(), false);
  VerifyPrefs(kInputId2, "", kInputId2);

  persistence.OnSessionStateChange(InputMethodManager::STATE_BROWSER_SCREEN);
  mock_manager_.SetCurrentInputMethodId(kInputId1);
  persistence.InputMethodChanged(&mock_manager_,
                                 ProfileManager::GetActiveUserProfile(), false);
  VerifyPrefs(kInputId1, kInputId2, kInputId2);
}

}  // namespace input_method
}  // namespace chromeos

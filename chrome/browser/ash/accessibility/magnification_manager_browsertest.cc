// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kTestUserName[] = "owner@invalid.domain";
constexpr char kTestUserGaiaId[] = "9876543210";

void SetMagnifierEnabled(bool enabled) {
  MagnificationManager::Get()->SetMagnifierEnabled(enabled);
}

void SetFullScreenMagnifierScale(double scale) {
  Shell::Get()->fullscreen_magnifier_controller()->SetScale(scale, false);
}

double GetFullScreenMagnifierScale() {
  return Shell::Get()->fullscreen_magnifier_controller()->GetScale();
}

void SetSavedFullScreenMagnifierScale(double scale) {
  MagnificationManager::Get()->SaveScreenMagnifierScale(scale);
}

double GetSavedFullScreenMagnifierScale() {
  return MagnificationManager::Get()->GetSavedScreenMagnifierScale();
}

bool IsMagnifierEnabled() {
  return MagnificationManager::Get()->IsMagnifierEnabled();
}

Profile* profile() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  return profile;
}

PrefService* prefs() {
  return user_prefs::UserPrefs::Get(profile());
}

void SetScreenMagnifierEnabledPref(bool enabled) {
  prefs()->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled, enabled);
}

void SetFullScreenMagnifierScalePref(double scale) {
  prefs()->SetDouble(prefs::kAccessibilityScreenMagnifierScale, scale);
}

bool GetScreenMagnifierEnabledFromPref() {
  return prefs()->GetBoolean(prefs::kAccessibilityScreenMagnifierEnabled);
}

// Creates and logs into a profile with account |account_id|, and makes sure
// that the profile is regarded as "non new" in the next login. This is used in
// PRE_XXX cases so that in the main XXX case we can test non new profiles.
void PrepareNonNewProfile(const AccountId& account_id) {
  session_manager::SessionManager::Get()->CreateSession(
      account_id, account_id.GetUserEmail(), false);
  // To prepare a non-new profile for tests, we must ensure the profile
  // directory and the preference files are created, because that's what
  // Profile::IsNewProfile() checks. CreateSession(), however, does not yet
  // create the profile directory, so create the profile actually here.
  profiles::testing::CreateProfileSync(
      g_browser_process->profile_manager(),
      BrowserContextHelper::Get()->GetBrowserContextPathByUserIdHash(
          user_manager::UserManager::Get()
              ->FindUser(account_id)
              ->username_hash()));
}

// Simulates how UserSessionManager starts a user session by loading user
// profile, notify user profile is loaded, and mark session as started.
void StartUserSession(const AccountId& account_id) {
  profiles::testing::CreateProfileSync(
      g_browser_process->profile_manager(),
      BrowserContextHelper::Get()->GetBrowserContextPathByUserIdHash(
          user_manager::UserManager::Get()
              ->FindUser(account_id)
              ->username_hash()));

  auto* session_manager = session_manager::SessionManager::Get();
  session_manager->NotifyUserProfileLoaded(account_id);
  session_manager->SessionStarted();
}

}  // namespace

class MockMagnificationObserver {
 public:
  MockMagnificationObserver() {
    AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
    CHECK(accessibility_manager);
    accessibility_subscription_ =
        accessibility_manager->RegisterCallback(base::BindRepeating(
            &MockMagnificationObserver::OnAccessibilityStatusChanged,
            base::Unretained(this)));
  }

  MockMagnificationObserver(const MockMagnificationObserver&) = delete;
  MockMagnificationObserver& operator=(const MockMagnificationObserver&) =
      delete;

  virtual ~MockMagnificationObserver() {}

  bool observed() const { return observed_; }
  bool observed_enabled() const { return observed_enabled_; }

  void reset() { observed_ = false; }

 private:
  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details) {
    if (details.notification_type ==
        AccessibilityNotificationType::kToggleScreenMagnifier) {
      observed_enabled_ = details.enabled;
      observed_ = true;
    }
  }

  bool observed_ = false;
  bool observed_enabled_ = false;

  base::CallbackListSubscription accessibility_subscription_;
};

class MagnificationManagerTest : public InProcessBrowserTest {
 protected:
  MagnificationManagerTest() {}

  MagnificationManagerTest(const MagnificationManagerTest&) = delete;
  MagnificationManagerTest& operator=(const MagnificationManagerTest&) = delete;

  ~MagnificationManagerTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUpOnMainThread() override {
    // Set the login-screen profile.
    MagnificationManager::Get()->SetProfileForTest(
        ProfileManager::GetActiveUserProfile());
  }

  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId);
};

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, PRE_LoginOffToOff) {
  // Create a new profile once, to run the test with non-new profile.
  PrepareNonNewProfile(test_account_id_);

  // Sets pref to explicitly disable the magnifier.
  SetScreenMagnifierEnabledPref(false);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginOffToOff) {
  // Confirms that magnifier is disabled on the login screen.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Disables magnifier on login screen.
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());

  // Logs in with existing profile.
  session_manager::SessionManager::Get()->CreateSession(test_account_id_,
                                                        kTestUserName, false);

  // Confirms that magnifier is still disabled just after login.
  EXPECT_FALSE(IsMagnifierEnabled());

  StartUserSession(test_account_id_);

  // Confirms that magnifier is still disabled just after session starts.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Enables magnifier.
  SetMagnifierEnabled(true);
  // Confirms that magnifier is enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_TRUE(GetScreenMagnifierEnabledFromPref());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, PRE_LoginFullToOff) {
  // Create a new profile once, to run the test with non-new profile.
  PrepareNonNewProfile(test_account_id_);

  // Sets pref to explicitly disable the magnifier.
  SetScreenMagnifierEnabledPref(false);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginFullToOff) {
  // Confirms that magnifier is disabled on the login screen.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Enables magnifier on login screen.
  SetMagnifierEnabled(true);
  SetFullScreenMagnifierScale(2.5);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());

  // Logs in (but the session is not started yet).
  session_manager::SessionManager::Get()->CreateSession(test_account_id_,
                                                        kTestUserName, false);

  // Confirms that magnifier is keeping enabled.
  EXPECT_TRUE(IsMagnifierEnabled());

  StartUserSession(test_account_id_);

  // Confirms that magnifier is disabled just after session start.
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_FALSE(GetScreenMagnifierEnabledFromPref());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, PRE_LoginOffToFull) {
  // Create a new profile once, to run the test with non-new profile.
  PrepareNonNewProfile(test_account_id_);

  // Sets prefs to explicitly enable the magnifier.
  SetScreenMagnifierEnabledPref(true);
  SetFullScreenMagnifierScalePref(2.5);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginOffToFull) {
  // Disables magnifier on login screen.
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());

  // Logs in (but the session is not started yet).
  session_manager::SessionManager::Get()->CreateSession(test_account_id_,
                                                        kTestUserName, false);

  // Confirms that magnifier is keeping disabled.
  EXPECT_FALSE(IsMagnifierEnabled());

  StartUserSession(test_account_id_);

  // Confirms that the magnifier is enabled and configured according to the
  // explicitly set prefs just after session start.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());
  EXPECT_TRUE(GetScreenMagnifierEnabledFromPref());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, PRE_LoginFullToFull) {
  // Create a new profile once, to run the test with non-new profile.
  PrepareNonNewProfile(test_account_id_);

  // Sets prefs to explicitly enable the magnifier.
  SetScreenMagnifierEnabledPref(true);
  SetFullScreenMagnifierScalePref(2.5);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginFullToFull) {
  // Enables magnifier on login screen.
  SetMagnifierEnabled(true);
  SetFullScreenMagnifierScale(3.0);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(3.0, GetFullScreenMagnifierScale());

  // Logs in (but the session is not started yet).
  session_manager::SessionManager::Get()->CreateSession(test_account_id_,
                                                        kTestUserName, false);

  // Confirms that magnifier is keeping enabled.
  EXPECT_TRUE(IsMagnifierEnabled());

  StartUserSession(test_account_id_);

  // Confirms that the magnifier is enabled and configured according to the
  // explicitly set prefs just after session start.
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());
  EXPECT_TRUE(GetScreenMagnifierEnabledFromPref());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, PRE_LoginFullToUnset) {
  // Creates a new profile once, to run the test with non-new profile.
  PrepareNonNewProfile(test_account_id_);
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginFullToUnset) {
  // Enables full screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());

  // Logs in (but the session is not started yet).
  session_manager::SessionManager::Get()->CreateSession(test_account_id_,
                                                        kTestUserName, false);

  // Confirms that magnifier is keeping enabled.
  EXPECT_TRUE(IsMagnifierEnabled());

  StartUserSession(test_account_id_);

  // Confirms that magnifier is disabled.
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_FALSE(GetScreenMagnifierEnabledFromPref());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, LoginAsNewUserUnset) {
  // Confirms that magnifier is disabled on the login screen.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Logs in (but the session is not started yet).
  session_manager::SessionManager::Get()->CreateSession(test_account_id_,
                                                        kTestUserName, false);

  // Confirms that magnifier is keeping disabled.
  EXPECT_FALSE(IsMagnifierEnabled());

  StartUserSession(test_account_id_);

  // Confirms that magnifier is keeping disabled.
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_FALSE(GetScreenMagnifierEnabledFromPref());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, TypePref) {
  // Logs in
  session_manager::SessionManager::Get()->CreateSession(test_account_id_,
                                                        kTestUserName, false);
  StartUserSession(test_account_id_);

  // Confirms that magnifier is disabled just after login.
  EXPECT_FALSE(IsMagnifierEnabled());

  // Sets the pref as true to enable magnifier.
  SetScreenMagnifierEnabledPref(true);
  // Confirms that magnifier is enabled.
  EXPECT_TRUE(IsMagnifierEnabled());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, ScalePref) {
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());

  // Sets 2.5x to the pref.
  SetSavedFullScreenMagnifierScale(2.5);

  // Enables full screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());

  // Confirms that 2.5x is restored.
  EXPECT_EQ(2.5, GetFullScreenMagnifierScale());

  // Sets the scale and confirms that the scale is saved to pref.
  SetFullScreenMagnifierScale(3.0);
  EXPECT_EQ(3.0, GetSavedFullScreenMagnifierScale());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, InvalidScalePref) {
  // TEST 1: Sets too small scale
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());

  // Sets too small value to the pref.
  SetSavedFullScreenMagnifierScale(0.5);

  // Enables full screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());

  // Confirms that the actual scale is set to the minimum scale.
  EXPECT_EQ(1.0, GetFullScreenMagnifierScale());

  // TEST 2: Sets too large scale
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMagnifierEnabled());

  // Sets too large value to the pref.
  SetSavedFullScreenMagnifierScale(50.0);

  // Enables full screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMagnifierEnabled());

  // Confirms that the actual scale is set to the maximum scale.
  EXPECT_EQ(20.0, GetFullScreenMagnifierScale());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, MagnificationObserver) {
  MockMagnificationObserver observer;

  EXPECT_FALSE(observer.observed());

  // Set full screen magnifier, and confirm the observer is called.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  observer.reset();

  // Set full screen magnifier again, and confirm the observer is not called.
  SetMagnifierEnabled(true);
  EXPECT_FALSE(observer.observed());
  observer.reset();
}

}  // namespace ash

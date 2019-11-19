// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/policy/policy_recommendation_restorer.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ash {

class PolicyRecommendationRestorerTest : public NoSessionAshTestBase {
 protected:
  PolicyRecommendationRestorerTest()
      : recommended_prefs_(new TestingPrefStore),
        prefs_(new sync_preferences::TestingPrefServiceSyncable(
            new TestingPrefStore,
            new TestingPrefStore,
            new TestingPrefStore,
            recommended_prefs_,
            new user_prefs::PrefRegistrySyncable,
            new PrefNotifierImpl)) {}
  ~PolicyRecommendationRestorerTest() override = default;

  // NoSessionAshTestBase override:
  void SetUp() override {
    TestSessionControllerClient::DisableAutomaticallyProvideSigninPref();
    NoSessionAshTestBase::SetUp();

    // Register sigin prefs but not connected to pref service yet. This allows
    // us set pref values before ash connects to pref service for testing.
    RegisterSigninProfilePrefs(prefs_->registry(), true /* for_test */);

    restorer_ = Shell::Get()->policy_recommendation_restorer();
  }

  void ConnectToSigninPrefService() {
    GetSessionControllerClient()->SetSigninScreenPrefService(
        base::WrapUnique(prefs_));
    ASSERT_EQ(Shell::Get()->session_controller()->GetSigninScreenPrefService(),
              prefs_);
    // Manually trigger a user activity, so that the delay is not skipped due to
    // no user input since a pref is started observing recommended value. See
    // PolicyRecommendationRestorer::Restore() for the information.
    ui::UserActivityDetector::Get()->HandleExternalUserActivity();
  }

  void SetRecommendedValues() {
    recommended_prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled,
                                   false);
    recommended_prefs_->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled,
                                   false);
    recommended_prefs_->SetBoolean(prefs::kAccessibilityHighContrastEnabled,
                                   false);
    recommended_prefs_->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled,
                                   false);
    recommended_prefs_->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled,
                                   false);
  }

  void SetUserSettings() {
    prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);
    prefs_->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled, true);
    prefs_->SetBoolean(prefs::kAccessibilityHighContrastEnabled, true);
    prefs_->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled, true);
    prefs_->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled, true);
  }

  void VerifyPrefFollowsUser(const std::string& pref_name,
                             const base::Value& expected_value) const {
    const sync_preferences::PrefServiceSyncable::Preference* pref =
        prefs_->FindPreference(pref_name);
    ASSERT_TRUE(pref);
    EXPECT_TRUE(pref->HasUserSetting());
    const base::Value* value = pref->GetValue();
    ASSERT_TRUE(value);
    EXPECT_TRUE(expected_value.Equals(value));
  }

  void VerifyPrefsFollowUser() const {
    VerifyPrefFollowsUser(prefs::kAccessibilityLargeCursorEnabled,
                          base::Value(true));
    VerifyPrefFollowsUser(prefs::kAccessibilitySpokenFeedbackEnabled,
                          base::Value(true));
    VerifyPrefFollowsUser(prefs::kAccessibilityHighContrastEnabled,
                          base::Value(true));
    VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierEnabled,
                          base::Value(true));
    VerifyPrefFollowsUser(prefs::kAccessibilityVirtualKeyboardEnabled,
                          base::Value(true));
  }

  void VerifyPrefFollowsRecommendation(
      const std::string& pref_name,
      const base::Value& expected_value) const {
    const sync_preferences::PrefServiceSyncable::Preference* pref =
        prefs_->FindPreference(pref_name);
    ASSERT_TRUE(pref);
    EXPECT_TRUE(pref->IsRecommended());
    EXPECT_FALSE(pref->HasUserSetting());
    const base::Value* value = pref->GetValue();
    ASSERT_TRUE(value);
    EXPECT_TRUE(expected_value.Equals(value));
  }

  void VerifyPrefsFollowRecommendation() const {
    VerifyPrefFollowsRecommendation(prefs::kAccessibilityLargeCursorEnabled,
                                    base::Value(false));
    VerifyPrefFollowsRecommendation(prefs::kAccessibilitySpokenFeedbackEnabled,
                                    base::Value(false));
    VerifyPrefFollowsRecommendation(prefs::kAccessibilityHighContrastEnabled,
                                    base::Value(false));
    VerifyPrefFollowsRecommendation(prefs::kAccessibilityScreenMagnifierEnabled,
                                    base::Value(false));
    VerifyPrefFollowsRecommendation(prefs::kAccessibilityVirtualKeyboardEnabled,
                                    base::Value(false));
  }

  bool RestoreTimerIsRunning() {
    return restorer_->restore_timer_for_test()->IsRunning();
  }

  // If restore timer is running, stops it, runs its task and returns true.
  // Otherwise, returns false.
  bool TriggerRestoreTimer() WARN_UNUSED_RESULT {
    if (!restorer_->restore_timer_for_test()->IsRunning())
      return false;

    restorer_->restore_timer_for_test()->FireNow();
    return true;
  }

  PolicyRecommendationRestorer* restorer_ = nullptr;

  // Ownerships are passed to SessionController.
  TestingPrefStore* recommended_prefs_;
  sync_preferences::TestingPrefServiceSyncable* prefs_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyRecommendationRestorerTest);
};

// Verifies that when no recommended values have been set, |restorer_| does not
// clear user settings on initialization and does not start a timer that will
// clear user settings eventually.
TEST_F(PolicyRecommendationRestorerTest, NoRecommendations) {
  SetUserSettings();

  ConnectToSigninPrefService();
  VerifyPrefsFollowUser();
  EXPECT_FALSE(RestoreTimerIsRunning());
}

// Verifies that when recommended values have been set, |restorer_| clears user
// settings on signing pref initialization.
TEST_F(PolicyRecommendationRestorerTest, RestoreOnSigninPrefInitialized) {
  SetRecommendedValues();
  SetUserSettings();

  ConnectToSigninPrefService();
  VerifyPrefsFollowRecommendation();
  EXPECT_FALSE(RestoreTimerIsRunning());
}

// Verifies that if recommended values change while the signin screen is being
// shown, a timer is started that will clear user settings eventually.
TEST_F(PolicyRecommendationRestorerTest,
       RestoreOnRecommendationChangeOnSigninScreen) {
  // No recommended values in observed prefs.
  ConnectToSigninPrefService();
  EXPECT_FALSE(RestoreTimerIsRunning());

  prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityLargeCursorEnabled,
                        base::Value(true));
  EXPECT_FALSE(RestoreTimerIsRunning());
  recommended_prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled,
                                 false);
  EXPECT_TRUE(TriggerRestoreTimer());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityLargeCursorEnabled,
                                  base::Value(false));

  prefs_->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilitySpokenFeedbackEnabled,
                        base::Value(true));
  recommended_prefs_->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled,
                                 false);
  EXPECT_TRUE(TriggerRestoreTimer());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilitySpokenFeedbackEnabled,
                                  base::Value(false));

  prefs_->SetBoolean(prefs::kAccessibilityHighContrastEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityHighContrastEnabled,
                        base::Value(true));
  recommended_prefs_->SetBoolean(prefs::kAccessibilityHighContrastEnabled,
                                 false);
  EXPECT_TRUE(TriggerRestoreTimer());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityHighContrastEnabled,
                                  base::Value(false));

  prefs_->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierEnabled,
                        base::Value(true));
  recommended_prefs_->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled,
                                 false);
  EXPECT_TRUE(TriggerRestoreTimer());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityScreenMagnifierEnabled,
                                  base::Value(false));

  prefs_->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityVirtualKeyboardEnabled,
                        base::Value(true));
  recommended_prefs_->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled,
                                 false);
  EXPECT_TRUE(TriggerRestoreTimer());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityVirtualKeyboardEnabled,
                                  base::Value(false));
}

// Verifies that if recommended values change while a user session is in
// progress, user settings are cleared immediately.
TEST_F(PolicyRecommendationRestorerTest,
       RestoreOnRecommendationChangeOnUserSession) {
  SetUserSettings();

  ConnectToSigninPrefService();
  SimulateUserLogin("user@test.com");

  VerifyPrefFollowsUser(prefs::kAccessibilityLargeCursorEnabled,
                        base::Value(true));
  recommended_prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled,
                                 false);
  EXPECT_FALSE(RestoreTimerIsRunning());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityLargeCursorEnabled,
                                  base::Value(false));

  VerifyPrefFollowsUser(prefs::kAccessibilitySpokenFeedbackEnabled,
                        base::Value(true));
  recommended_prefs_->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled,
                                 false);
  EXPECT_FALSE(RestoreTimerIsRunning());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilitySpokenFeedbackEnabled,
                                  base::Value(false));

  VerifyPrefFollowsUser(prefs::kAccessibilityHighContrastEnabled,
                        base::Value(true));
  recommended_prefs_->SetBoolean(prefs::kAccessibilityHighContrastEnabled,
                                 false);
  EXPECT_FALSE(RestoreTimerIsRunning());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityHighContrastEnabled,
                                  base::Value(false));

  VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierEnabled,
                        base::Value(true));
  recommended_prefs_->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled,
                                 false);
  EXPECT_FALSE(RestoreTimerIsRunning());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityScreenMagnifierEnabled,
                                  base::Value(false));

  VerifyPrefFollowsUser(prefs::kAccessibilityVirtualKeyboardEnabled,
                        base::Value(true));
  recommended_prefs_->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled,
                                 false);
  EXPECT_FALSE(RestoreTimerIsRunning());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityVirtualKeyboardEnabled,
                                  base::Value(false));
}

// Verifies that if no recommended values have been set and user settings
// change, the user settings are not cleared immediately and no timer is started
// that will clear the user settings eventually.
TEST_F(PolicyRecommendationRestorerTest, DoNothingOnUserChange) {
  ConnectToSigninPrefService();

  // a11y mono audio is not observing recommended values in production.
  prefs_->SetBoolean(prefs::kAccessibilityMonoAudioEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityMonoAudioEnabled,
                        base::Value(true));
  EXPECT_FALSE(RestoreTimerIsRunning());
}

TEST_F(PolicyRecommendationRestorerTest, RestoreOnUserChange) {
  SetRecommendedValues();
  ConnectToSigninPrefService();

  EXPECT_FALSE(RestoreTimerIsRunning());
  prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityLargeCursorEnabled,
                        base::Value(true));
  EXPECT_TRUE(TriggerRestoreTimer());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityLargeCursorEnabled,
                                  base::Value(false));

  EXPECT_FALSE(RestoreTimerIsRunning());
  prefs_->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilitySpokenFeedbackEnabled,
                        base::Value(true));
  EXPECT_TRUE(TriggerRestoreTimer());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilitySpokenFeedbackEnabled,
                                  base::Value(false));

  EXPECT_FALSE(RestoreTimerIsRunning());
  prefs_->SetBoolean(prefs::kAccessibilityHighContrastEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityHighContrastEnabled,
                        base::Value(true));
  EXPECT_TRUE(TriggerRestoreTimer());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityHighContrastEnabled,
                                  base::Value(false));

  EXPECT_FALSE(RestoreTimerIsRunning());
  prefs_->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityScreenMagnifierEnabled,
                        base::Value(true));
  EXPECT_TRUE(TriggerRestoreTimer());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityScreenMagnifierEnabled,
                                  base::Value(false));

  EXPECT_FALSE(RestoreTimerIsRunning());
  prefs_->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled, true);
  VerifyPrefFollowsUser(prefs::kAccessibilityVirtualKeyboardEnabled,
                        base::Value(true));
  EXPECT_TRUE(TriggerRestoreTimer());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityVirtualKeyboardEnabled,
                                  base::Value(false));

  EXPECT_FALSE(RestoreTimerIsRunning());
}

// Verifies that if recommended values have not been set, user settings have
// changed and a session is then started, the user settings are not cleared
// immediately.
TEST_F(PolicyRecommendationRestorerTest, DoNothingOnSessionStart) {
  ConnectToSigninPrefService();
  SetUserSettings();

  SimulateUserLogin("user@test.com");
  VerifyPrefsFollowUser();
  EXPECT_FALSE(RestoreTimerIsRunning());
}

// Verifies that user activity resets the timer which clears user settings.
TEST_F(PolicyRecommendationRestorerTest, UserActivityResetsTimer) {
  recommended_prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled,
                                 false);
  ConnectToSigninPrefService();

  prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);
  EXPECT_TRUE(RestoreTimerIsRunning());

  // Notify that there is user activity.
  restorer_->OnUserActivity(nullptr);
  VerifyPrefFollowsUser(prefs::kAccessibilityLargeCursorEnabled,
                        base::Value(true));

  // Trigger reset timer to timeout.
  EXPECT_TRUE(TriggerRestoreTimer());
  VerifyPrefFollowsRecommendation(prefs::kAccessibilityLargeCursorEnabled,
                                  base::Value(false));
  EXPECT_FALSE(RestoreTimerIsRunning());
}

}  // namespace ash

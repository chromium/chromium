// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/speak_on_mute_detection_privacy_switch_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"

namespace ash {

class PrivacyHubSpeakOnMuteControllerTest : public AshTestBase {
 public:
  PrivacyHubSpeakOnMuteControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // We need the privacy hub feature flag to have the controller constructed,
    // and the video conference feature flag together with the camera effects
    // switch to enable video conference.
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kCrosPrivacyHub,
         features::kFeatureManagementVideoConference},
        {});
  }

  ~PrivacyHubSpeakOnMuteControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    speak_on_mute_controller_ =
        Shell::Get()->privacy_hub_controller()->speak_on_mute_controller();
  }

  void SetUserPref(bool enabled) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        prefs::kUserSpeakOnMuteDetectionEnabled, enabled);
  }

  bool GetUserPref() const {
    return Shell::Get()
        ->session_controller()
        ->GetActivePrefService()
        ->GetBoolean(prefs::kUserSpeakOnMuteDetectionEnabled);
  }

  bool IsSpeakOnMuteDetectionOn() const {
    return FakeCrasAudioClient::Get()->speak_on_mute_detection_enabled();
  }

 private:
  raw_ptr<SpeakOnMuteDetectionPrivacySwitchController, DanglingUntriaged>
      speak_on_mute_controller_;
  // Instantiates a fake controller (the real one is created in
  // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
  // ash unit tests).
  FakeVideoConferenceTrayController fake_video_conference_tray_controller_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrivacyHubSpeakOnMuteControllerTest, SetSpeakOnMuteOnLogin) {
  // Checks that it should work for both when the first user's default pref is
  // true or false.
  for (bool speak_on_mute_enabled : {false, true}) {
    // Sets the pref for the default user.
    SetUserPref(speak_on_mute_enabled);
    ASSERT_EQ(IsSpeakOnMuteDetectionOn(), speak_on_mute_enabled);
    const AccountId user1_account_id =
        Shell::Get()->session_controller()->GetActiveAccountId();

    // Sets the pref for the second user.
    SimulateUserLogin("other@user.test");
    SetUserPref(!speak_on_mute_enabled);
    EXPECT_EQ(IsSpeakOnMuteDetectionOn(), !speak_on_mute_enabled);

    // Switching back to the previous user will also update the speak-on-mute
    // detection state during the login.
    GetSessionControllerClient()->SwitchActiveUser(user1_account_id);
    EXPECT_EQ(IsSpeakOnMuteDetectionOn(), speak_on_mute_enabled);

    // Clears all logins and re-logins the default user.
    ClearLogin();
    SimulateUserLogin(user1_account_id);
  }
}

TEST_F(PrivacyHubSpeakOnMuteControllerTest, OnPreferenceChanged) {
  for (bool speak_on_mute_enabled : {false, true, false}) {
    SetUserPref(speak_on_mute_enabled);
    EXPECT_EQ(IsSpeakOnMuteDetectionOn(), speak_on_mute_enabled);
  }
}

}  // namespace ash

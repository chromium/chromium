// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/microphone_mute_notification_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/account_id/account_id.h"

namespace ash {

namespace {

class FakeMicrophoneMuteNotificationDelegate
    : public MicrophoneMuteNotificationDelegate {
 public:
  absl::optional<std::u16string> GetAppAccessingMicrophone() override {
    return absl::nullopt;
  }
};

}  // namespace

class MicrophonePrivacySwitchControllerTest : public AshTestBase {
 public:
  MicrophonePrivacySwitchControllerTest() = default;
  ~MicrophonePrivacySwitchControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // This makes sure a global instance of MicrophoneMuteNotificationDelegate
    // is created before running tests.
    delegate_ = std::make_unique<FakeMicrophoneMuteNotificationDelegate>();
  }

 protected:
  void SetUserPref(bool allowed) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        prefs::kUserMicrophoneAllowed, allowed);
  }

  bool GetUserPref() {
    return Shell::Get()
        ->session_controller()
        ->GetActivePrefService()
        ->GetBoolean(prefs::kUserMicrophoneAllowed);
  }

 private:
  std::unique_ptr<FakeMicrophoneMuteNotificationDelegate> delegate_;
};

TEST_F(MicrophonePrivacySwitchControllerTest, SetSystemMuteOnLogin) {
  for (bool microphone_allowed : {false, true, false}) {
    const bool microphone_muted = !microphone_allowed;
    SetUserPref(microphone_allowed);
    ASSERT_EQ(CrasAudioHandler::Get()->IsInputMuted(), microphone_muted);
    const AccountId user1_account_id =
        Shell::Get()->session_controller()->GetActiveAccountId();

    SimulateUserLogin("other@user.test");
    SetUserPref(microphone_muted);
    EXPECT_EQ(CrasAudioHandler::Get()->IsInputMuted(), microphone_allowed);

    SimulateUserLogin(user1_account_id);
    EXPECT_EQ(CrasAudioHandler::Get()->IsInputMuted(), microphone_muted);
  }
}

TEST_F(MicrophonePrivacySwitchControllerTest, OnPreferenceChanged) {
  for (bool microphone_allowed : {false, true, false}) {
    SetUserPref(microphone_allowed);
    EXPECT_EQ(CrasAudioHandler::Get()->IsInputMuted(), !microphone_allowed);
  }
}

TEST_F(MicrophonePrivacySwitchControllerTest, OnInputMuteChanged) {
  for (bool microphone_muted : {false, true, false}) {
    const bool microphone_allowed = !microphone_muted;

    CrasAudioHandler::Get()->SetInputMute(microphone_muted);
    EXPECT_EQ(GetUserPref(), microphone_allowed);
  }
}

}  // namespace ash

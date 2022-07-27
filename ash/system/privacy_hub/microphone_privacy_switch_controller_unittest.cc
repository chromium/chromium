// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"

#include "ash/components/audio/cras_audio_handler.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/microphone_mute_notification_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"

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

    // This makes sure a globale instance of MicrophoneMuteNotificationDelegate
    // is created before running tests.
    delegate_ = std::make_unique<FakeMicrophoneMuteNotificationDelegate>();
  }

 protected:
  void SetUserPref(bool allowed) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        prefs::kUserMicrophoneAllowed, allowed);
  }

 private:
  std::unique_ptr<FakeMicrophoneMuteNotificationDelegate> delegate_;
};

TEST_F(MicrophonePrivacySwitchControllerTest, OnPreferenceChanged) {
  static constexpr bool user_preferences[] = {false, true, false};

  for (bool microphone_allowed : user_preferences) {
    SetUserPref(microphone_allowed);
    EXPECT_EQ(CrasAudioHandler::Get()->IsInputMuted(), !microphone_allowed);
  }
}

}  // namespace ash

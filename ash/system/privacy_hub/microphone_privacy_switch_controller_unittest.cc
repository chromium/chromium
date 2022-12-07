// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"

#include <string>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/privacy_hub_delegate.h"
#include "ash/public/cpp/sensor_disabled_notification_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using testing::_;

namespace {

class FakeSensorDisabledNotificationDelegate
    : public SensorDisabledNotificationDelegate {
 public:
  std::vector<std::u16string> GetAppsAccessingSensor(Sensor sensor) override {
    return {};
  }
};

class MockFrontendAPI : public PrivacyHubDelegate {
 public:
  MOCK_METHOD(void, AvailabilityOfMicrophoneChanged, (bool), (override));
  MOCK_METHOD(void, MicrophoneHardwareToggleChanged, (bool), (override));
  void CameraHardwareToggleChanged(
      cros::mojom::CameraPrivacySwitchState state) override {}
};

}  // namespace

class PrivacyHubMicrophoneControllerTest : public AshTestBase {
 public:
  PrivacyHubMicrophoneControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kCrosPrivacyHub);
  }
  ~PrivacyHubMicrophoneControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // This makes sure a global instance of SensorDisabledNotificationDelegate
    // is created before running tests.
    delegate_ = std::make_unique<FakeSensorDisabledNotificationDelegate>();
    Shell::Get()->privacy_hub_controller()->set_frontend(&mock_frontend_);
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

  ::testing::NiceMock<MockFrontendAPI> mock_frontend_;

 private:
  std::unique_ptr<FakeSensorDisabledNotificationDelegate> delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrivacyHubMicrophoneControllerTest, SetSystemMuteOnLogin) {
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

TEST_F(PrivacyHubMicrophoneControllerTest, OnPreferenceChanged) {
  for (bool microphone_allowed : {false, true, false}) {
    SetUserPref(microphone_allowed);
    EXPECT_EQ(CrasAudioHandler::Get()->IsInputMuted(), !microphone_allowed);
  }
}

TEST_F(PrivacyHubMicrophoneControllerTest, OnInputMuteChanged) {
  for (bool microphone_muted : {false, true, false}) {
    const bool microphone_allowed = !microphone_muted;

    CrasAudioHandler::Get()->SetInputMute(
        microphone_muted, CrasAudioHandler::InputMuteChangeMethod::kOther);
    EXPECT_EQ(GetUserPref(), microphone_allowed);
  }
}

TEST_F(PrivacyHubMicrophoneControllerTest, OnAudioNodesChanged) {
  EXPECT_CALL(mock_frontend_, AvailabilityOfMicrophoneChanged(_));
  Shell::Get()
      ->privacy_hub_controller()
      ->microphone_controller()
      .OnAudioNodesChanged();
}

TEST_F(PrivacyHubMicrophoneControllerTest, OnMicrophoneMuteSwitchValueChanged) {
  EXPECT_CALL(mock_frontend_, MicrophoneHardwareToggleChanged(_));
  Shell::Get()
      ->privacy_hub_controller()
      ->microphone_controller()
      .OnMicrophoneMuteSwitchValueChanged(true);
}

}  // namespace ash

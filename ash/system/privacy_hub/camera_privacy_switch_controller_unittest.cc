// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"

using testing::_;

namespace ash {

namespace {

class MockSwitchAPI : public CameraPrivacySwitchAPI {
 public:
  MOCK_METHOD(void,
              SetCameraSWPrivacySwitch,
              (CameraSWPrivacySwitchSetting),
              (override));
};

class MockFrontendAPI : public PrivacyHubDelegate {
 public:
  MOCK_METHOD(void,
              CameraHardwareToggleChanged,
              (cros::mojom::CameraPrivacySwitchState state),
              (override));
  void AvailabilityOfMicrophoneChanged(bool) override {}
  void MicrophoneHardwareToggleChanged(bool) override {}
};

}  // namespace

class PrivacyHubCameraControllerTests : public AshTestBase {
 protected:
  PrivacyHubCameraControllerTests() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kCrosPrivacyHub);
  }

  void SetUserPref(bool allowed) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        prefs::kUserCameraAllowed, allowed);
  }

  bool GetUserPref() const {
    return Shell::Get()
        ->session_controller()
        ->GetActivePrefService()
        ->GetBoolean(prefs::kUserCameraAllowed);
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    auto mock_switch = std::make_unique<::testing::NiceMock<MockSwitchAPI>>();
    mock_switch_ = mock_switch.get();

    Shell::Get()->privacy_hub_controller()->set_frontend(&mock_frontend_);
    controller_ = &Shell::Get()->privacy_hub_controller()->camera_controller();
    controller_->SetCameraPrivacySwitchAPIForTest(std::move(mock_switch));
  }

  ::testing::NiceMock<MockFrontendAPI> mock_frontend_;
  ::testing::NiceMock<MockSwitchAPI>* mock_switch_;
  CameraPrivacySwitchController* controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test reaction on UI action.
TEST_F(PrivacyHubCameraControllerTests, UIAction) {
  const std::vector<bool> user_pref_sequence{false, true, true, false, true};
  const int number_of_changes = [&]() {
    int cnt = 0;
    bool current_val = true;  // default value for camera-enabled is true
    for (const bool pref_val : user_pref_sequence) {
      if (pref_val != current_val) {
        ++cnt;
        current_val = pref_val;
      }
    }
    return cnt;
  }();

  CameraSWPrivacySwitchSetting captured_val;
  EXPECT_CALL(*mock_switch_, SetCameraSWPrivacySwitch(_))
      .Times(number_of_changes)
      .WillRepeatedly(testing::SaveArg<0>(&captured_val));

  for (const bool pref_val : user_pref_sequence) {
    SetUserPref(pref_val);
    // User toggle ON means the camera is DISABLED.
    const CameraSWPrivacySwitchSetting expected_val =
        pref_val ? CameraSWPrivacySwitchSetting::kEnabled
                 : CameraSWPrivacySwitchSetting::kDisabled;
    EXPECT_EQ(captured_val, expected_val);
  }
}

TEST_F(PrivacyHubCameraControllerTests, OnCameraHardwarePrivacySwitchChanged) {
  EXPECT_CALL(mock_frontend_, CameraHardwareToggleChanged(
                                  cros::mojom::CameraPrivacySwitchState::OFF));
  EXPECT_CALL(mock_frontend_, CameraHardwareToggleChanged(
                                  cros::mojom::CameraPrivacySwitchState::ON));
  CameraPrivacySwitchController& controller =
      Shell::Get()->privacy_hub_controller()->camera_controller();
  SetUserPref(true);

  controller.OnCameraHWPrivacySwitchStatusChanged(
      0, cros::mojom::CameraPrivacySwitchState::OFF);
  EXPECT_EQ(cros::mojom::CameraPrivacySwitchState::OFF,
            controller.HWSwitchState());
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId));

  controller.OnCameraHWPrivacySwitchStatusChanged(
      0, cros::mojom::CameraPrivacySwitchState::ON);
  EXPECT_EQ(cros::mojom::CameraPrivacySwitchState::ON,
            controller.HWSwitchState());

  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  EXPECT_TRUE(message_center->FindNotificationById(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId));
  EXPECT_TRUE(GetUserPref());
  message_center->ClickOnNotificationButton(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId, 0);
  EXPECT_FALSE(GetUserPref());
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId));
}

TEST_F(PrivacyHubCameraControllerTests, CameraOffNotificationRemoveViaClick) {
  SetUserPref(false);
  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  ASSERT_TRUE(message_center);
  ASSERT_FALSE(
      message_center->FindNotificationById(kPrivacyHubCameraOffNotificationId));

  // Emulate camera activity
  controller_->OnActiveClientChange(cros::mojom::CameraClientType::ASH_CHROME,
                                    true);
  // A notification should be fired.
  EXPECT_TRUE(
      message_center->FindNotificationById(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(GetUserPref());

  // Enabling camera via clicking on the button should clear the notification
  message_center->ClickOnNotificationButton(kPrivacyHubCameraOffNotificationId,
                                            0);
  EXPECT_TRUE(GetUserPref());
  EXPECT_FALSE(
      message_center->FindNotificationById(kPrivacyHubCameraOffNotificationId));
}

TEST_F(PrivacyHubCameraControllerTests,
       CameraOffNotificationRemoveViaUserPref) {
  SetUserPref(false);
  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  ASSERT_TRUE(message_center);
  ASSERT_FALSE(
      message_center->FindNotificationById(kPrivacyHubCameraOffNotificationId));

  // Emulate camera activity
  controller_->OnActiveClientChange(cros::mojom::CameraClientType::ASH_CHROME,
                                    true);
  // A notification should be fired.
  EXPECT_TRUE(
      message_center->FindNotificationById(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(GetUserPref());

  // Enabling camera via the user pref should clear the notification
  SetUserPref(true);
  EXPECT_TRUE(GetUserPref());
  EXPECT_FALSE(
      message_center->FindNotificationById(kPrivacyHubCameraOffNotificationId));
}

TEST_F(PrivacyHubCameraControllerTests, InSessionSwitchNotification) {
  SetUserPref(true);
  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  ASSERT_TRUE(message_center);
  message_center->RemoveNotification(kPrivacyHubCameraOffNotificationId, false);

  // Emulate camera activity
  controller_->OnActiveClientChange(cros::mojom::CameraClientType::ASH_CHROME,
                                    true);
  // Disable camera
  SetUserPref(false);

  // A notification should be fired.
  EXPECT_TRUE(
      message_center->FindNotificationById(kPrivacyHubCameraOffNotificationId));
  EXPECT_FALSE(GetUserPref());

  // Enabling camera via clicking on the button should clear the notification
  message_center->ClickOnNotificationButton(kPrivacyHubCameraOffNotificationId,
                                            0);
  EXPECT_TRUE(GetUserPref());
  EXPECT_FALSE(
      message_center->FindNotificationById(kPrivacyHubCameraOffNotificationId));
}

}  // namespace ash

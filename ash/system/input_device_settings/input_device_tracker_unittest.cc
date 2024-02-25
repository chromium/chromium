// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_tracker.h"

#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"

namespace ash {

using InputDeviceCategory = InputDeviceTracker::InputDeviceCategory;

namespace {
const std::string_view kDeviceKey1 = "5555:1111";
const std::string_view kDeviceKey2 = "3333:22aa";
const std::string_view kDeviceKey3 = "aa22:eeff";

constexpr char kUserEmail1[] = "email1@peripherals";
constexpr char kUserEmail2[] = "email2@peripherals";
}  // namespace

class InputDeviceTrackerTest
    : public AshTestBase,
      public ::testing::WithParamInterface<
          std::pair<InputDeviceCategory, std::string_view>> {
 public:
  InputDeviceTrackerTest() = default;
  InputDeviceTrackerTest(const InputDeviceTrackerTest&) = delete;
  InputDeviceTrackerTest& operator=(const InputDeviceTrackerTest&) = delete;
  ~InputDeviceTrackerTest() override = default;

  // testing::Test:
  void SetUp() override {
    feature_list_.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
    AshTestBase::SetUp();
    std::tie(category_, pref_path_) = GetParam();
    tracker_ = std::make_unique<InputDeviceTracker>();
    SimulateUserLogin(GetAccountId(kUserEmail1));
  }

  void TearDown() override {
    tracker_.reset();
    AshTestBase::TearDown();
  }

  AccountId GetAccountId(std::string_view email) {
    return AccountId::FromUserEmail(std::string(email));
  }

  void CheckObservedDevicesList(
      std::vector<std::string_view> expected_devices) {
    pref_service_ = Shell::Get()->session_controller()->GetActivePrefService();
    const auto& list = pref_service_->GetList(pref_path_);
    EXPECT_EQ(expected_devices.size(), list.size());

    for (const auto& device : expected_devices) {
      EXPECT_TRUE(base::Contains(list, device));
    }
  }

  void CallOnDeviceConnected(std::string_view device_key) {
    switch (category_) {
      case InputDeviceCategory::kKeyboard: {
        mojom::Keyboard keyboard;
        keyboard.device_key = std::string(device_key);
        tracker_->OnKeyboardConnected(keyboard);
        break;
      }
      case InputDeviceCategory::kMouse: {
        mojom::Mouse mouse;
        mouse.device_key = std::string(device_key);
        tracker_->OnMouseConnected(mouse);
        break;
      }
      case InputDeviceCategory::kTouchpad: {
        mojom::Touchpad touchpad;
        touchpad.device_key = std::string(device_key);
        tracker_->OnTouchpadConnected(touchpad);
        break;
      }
      case InputDeviceCategory::kPointingStick:
        mojom::PointingStick pointing_stick;
        pointing_stick.device_key = std::string(device_key);
        tracker_->OnPointingStickConnected(pointing_stick);
        break;
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<InputDeviceTracker> tracker_;
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;

  InputDeviceCategory category_;
  std::string_view pref_path_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    InputDeviceTrackerTest,
    testing::ValuesIn(
        std::vector<std::pair<InputDeviceCategory, std::string_view>>{
            {InputDeviceCategory::kKeyboard,
             prefs::kKeyboardObservedDevicesPref},
            {InputDeviceCategory::kMouse, prefs::kMouseObservedDevicesPref},
            {InputDeviceCategory::kTouchpad,
             prefs::kTouchpadObservedDevicesPref},
            {InputDeviceCategory::kPointingStick,
             prefs::kPointingStickObservedDevicesPref}}));

TEST_P(InputDeviceTrackerTest, RecordDevices) {
  CallOnDeviceConnected(kDeviceKey1);
  CallOnDeviceConnected(kDeviceKey2);
  CallOnDeviceConnected(kDeviceKey3);
  CheckObservedDevicesList({kDeviceKey1, kDeviceKey2, kDeviceKey3});
}

TEST_P(InputDeviceTrackerTest, RecordDuplicateDevices) {
  CallOnDeviceConnected(kDeviceKey1);
  CallOnDeviceConnected(kDeviceKey2);
  CallOnDeviceConnected(kDeviceKey3);

  CheckObservedDevicesList({kDeviceKey1, kDeviceKey2, kDeviceKey3});

  CallOnDeviceConnected(kDeviceKey1);
  CallOnDeviceConnected(kDeviceKey2);
  CallOnDeviceConnected(kDeviceKey3);

  CheckObservedDevicesList({kDeviceKey1, kDeviceKey2, kDeviceKey3});
}

TEST_P(InputDeviceTrackerTest, RecordDevicesTwoUsers) {
  CallOnDeviceConnected(kDeviceKey1);
  CallOnDeviceConnected(kDeviceKey2);
  CallOnDeviceConnected(kDeviceKey3);

  CheckObservedDevicesList({kDeviceKey1, kDeviceKey2, kDeviceKey3});

  // Switch account
  SimulateUserLogin(GetAccountId(kUserEmail2));
  CheckObservedDevicesList({});

  CallOnDeviceConnected(kDeviceKey1);
  CheckObservedDevicesList({kDeviceKey1});

  CallOnDeviceConnected(kDeviceKey2);
  CheckObservedDevicesList({kDeviceKey1, kDeviceKey2});

  CallOnDeviceConnected(kDeviceKey3);
  CheckObservedDevicesList({kDeviceKey1, kDeviceKey2, kDeviceKey3});
}

TEST_P(InputDeviceTrackerTest, WasDevicePreviouslyConnected) {
  EXPECT_FALSE(tracker_->WasDevicePreviouslyConnected(category_, kDeviceKey1));
  CallOnDeviceConnected(kDeviceKey1);
  EXPECT_TRUE(tracker_->WasDevicePreviouslyConnected(category_, kDeviceKey1));
}

}  // namespace ash

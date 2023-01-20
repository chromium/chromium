// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_tracker.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/strings/string_piece_forward.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"

namespace ash {

using InputDeviceCategory = InputDeviceTracker::InputDeviceCategory;

namespace {
const base::StringPiece kDeviceKey1 = "5555:1111";
const base::StringPiece kDeviceKey2 = "3333:22aa";
const base::StringPiece kDeviceKey3 = "aa22:eeff";

constexpr char kUserEmail1[] = "email1@peripherals";
constexpr char kUserEmail2[] = "email2@peripherals";
}  // namespace

class InputDeviceTrackerTest
    : public AshTestBase,
      public ::testing::WithParamInterface<
          std::pair<InputDeviceCategory, base::StringPiece>> {
 public:
  InputDeviceTrackerTest() = default;
  InputDeviceTrackerTest(const InputDeviceTrackerTest&) = delete;
  InputDeviceTrackerTest& operator=(const InputDeviceTrackerTest&) = delete;
  ~InputDeviceTrackerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    std::tie(category_, pref_path_) = GetParam();
    tracker_ = std::make_unique<InputDeviceTracker>();
    SimulateUserLogin(GetAccountId(kUserEmail1));
  }

  void TearDown() override {
    tracker_.reset();
    AshTestBase::TearDown();
  }

  AccountId GetAccountId(base::StringPiece email) {
    return AccountId::FromUserEmail(std::string(email));
  }

  void CheckObservedDevicesList(
      std::vector<base::StringPiece> expected_devices) {
    pref_service_ = Shell::Get()->session_controller()->GetActivePrefService();
    const auto& list = pref_service_->GetList(pref_path_);
    EXPECT_EQ(expected_devices.size(), list.size());

    for (const auto& device : expected_devices) {
      EXPECT_TRUE(base::Contains(list, device));
    }
  }

  // TODO(dpad): Implement for mouse/touchpad/pointing stick once mojo objects
  // are available.
  void CallOnDeviceConnected(base::StringPiece device_key) {
    switch (category_) {
      case InputDeviceCategory::kKeyboard: {
        mojom::Keyboard keyboard;
        keyboard.device_key = std::string(device_key);
        tracker_->OnKeyboardConnected(keyboard);
        break;
      }
      case InputDeviceCategory::kMouse:
      case InputDeviceCategory::kTouchpad:
      case InputDeviceCategory::kPointingStick:
        NOTIMPLEMENTED();
        break;
    }
  }

 protected:
  std::unique_ptr<InputDeviceTracker> tracker_;
  base::raw_ptr<PrefService> pref_service_;

  InputDeviceCategory category_;
  base::StringPiece pref_path_;
};

// TODO(dpad): Add in mouse/touchpad/pointing stick once implemented.
INSTANTIATE_TEST_SUITE_P(
    ,
    InputDeviceTrackerTest,
    testing::ValuesIn(
        std::vector<std::pair<InputDeviceCategory, base::StringPiece>>{
            {InputDeviceCategory::kKeyboard,
             prefs::kKeyboardObservedDevicesPref}}));

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
}  // namespace ash

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_tracker_impl.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/strings/string_piece_forward.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {
const base::StringPiece kDeviceKey1 = "5555:1111";
const base::StringPiece kDeviceKey2 = "3333:22aa";
const base::StringPiece kDeviceKey3 = "aa22:eeff";

constexpr char kUserEmail[] = "email@peripherals";
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
    pref_service_ = Shell::Get()->session_controller()->GetActivePrefService();
    tracker_ = std::make_unique<InputDeviceTrackerImpl>();
    tracker_->Init(pref_service_);
  }

  void TearDown() override {
    tracker_.reset();
    AshTestBase::TearDown();
  }

  AccountId GetAccountId() { return AccountId::FromUserEmail(kUserEmail); }

  void CheckObservedDevicesList(
      std::vector<base::StringPiece> expected_devices) {
    const auto& list = pref_service_->GetList(pref_path_);
    EXPECT_EQ(expected_devices.size(), list.size());

    for (const auto& device : expected_devices) {
      EXPECT_TRUE(base::Contains(list, device));
    }
  }

 protected:
  std::unique_ptr<InputDeviceTrackerImpl> tracker_;
  base::raw_ptr<PrefService> pref_service_;

  InputDeviceCategory category_;
  base::StringPiece pref_path_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    InputDeviceTrackerTest,
    testing::ValuesIn(
        std::vector<std::pair<InputDeviceCategory, base::StringPiece>>{
            {InputDeviceCategory::kMouse, prefs::kMouseObservedDevicesPref},
            {InputDeviceCategory::kTouchpad,
             prefs::kTouchpadObservedDevicesPref},
            {InputDeviceCategory::kPointingStick,
             prefs::kPointingStickObservedDevicesPref},
            {InputDeviceCategory::kKeyboard,
             prefs::kKeyboardObservedDevicesPref}}));

TEST_P(InputDeviceTrackerTest, RecordDevices) {
  tracker_->RecordDeviceConnected(category_, kDeviceKey1);
  tracker_->RecordDeviceConnected(category_, kDeviceKey2);
  tracker_->RecordDeviceConnected(category_, kDeviceKey3);
  CheckObservedDevicesList({kDeviceKey1, kDeviceKey2, kDeviceKey3});
}

TEST_P(InputDeviceTrackerTest, RecordDuplicateDevices) {
  tracker_->RecordDeviceConnected(category_, kDeviceKey1);
  tracker_->RecordDeviceConnected(category_, kDeviceKey2);
  tracker_->RecordDeviceConnected(category_, kDeviceKey3);

  CheckObservedDevicesList({kDeviceKey1, kDeviceKey2, kDeviceKey3});

  tracker_->RecordDeviceConnected(category_, kDeviceKey1);
  tracker_->RecordDeviceConnected(category_, kDeviceKey2);
  tracker_->RecordDeviceConnected(category_, kDeviceKey3);

  CheckObservedDevicesList({kDeviceKey1, kDeviceKey2, kDeviceKey3});
}

TEST_P(InputDeviceTrackerTest, RecordDevicesTwoUsers) {
  tracker_->RecordDeviceConnected(category_, kDeviceKey1);
  tracker_->RecordDeviceConnected(category_, kDeviceKey2);
  tracker_->RecordDeviceConnected(category_, kDeviceKey3);

  CheckObservedDevicesList({kDeviceKey1, kDeviceKey2, kDeviceKey3});

  // Switch account
  SimulateUserLogin(GetAccountId());
  pref_service_ = Shell::Get()->session_controller()->GetActivePrefService();
  tracker_->Init(pref_service_);

  CheckObservedDevicesList({});

  tracker_->RecordDeviceConnected(category_, kDeviceKey1);
  CheckObservedDevicesList({kDeviceKey1});

  tracker_->RecordDeviceConnected(category_, kDeviceKey2);
  CheckObservedDevicesList({kDeviceKey1, kDeviceKey2});

  tracker_->RecordDeviceConnected(category_, kDeviceKey3);
  CheckObservedDevicesList({kDeviceKey1, kDeviceKey2, kDeviceKey3});
}
}  // namespace ash

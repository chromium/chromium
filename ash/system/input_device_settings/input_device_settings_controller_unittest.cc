// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler.h"
#include "ash/test/ash_test_base.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/functional.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/events/devices/input_device.h"

namespace ash {

using DeviceId = InputDeviceSettingsController::DeviceId;

namespace {
const ui::InputDevice kSampleKeyboardInternal = {5, ui::INPUT_DEVICE_INTERNAL,
                                                 "kSampleKeyboardInternal"};
const ui::InputDevice kSampleKeyboardBluetooth = {
    10, ui::INPUT_DEVICE_BLUETOOTH, "kSampleKeyboardBluetooth"};
const ui::InputDevice kSampleKeyboardUsb = {15, ui::INPUT_DEVICE_USB,
                                            "kSampleKeyboardUsb"};

constexpr char kUserEmail[] = "example1@abc.com";
}  // namespace

class FakeKeyboardPrefHandler : public KeyboardPrefHandler {
 public:
  void InitializeKeyboardSettings(PrefService* pref_service,
                                  mojom::Keyboard* keyboard) override {
    num_keyboard_settings_initialized_++;
  }
  void UpdateKeyboardSettings(PrefService* pref_service,
                              const mojom::Keyboard& keyboard) override {}

  uint32_t num_keyboard_settings_initialized() {
    return num_keyboard_settings_initialized_;
  }

 private:
  uint32_t num_keyboard_settings_initialized_ = 0;
};

class FakeInputDeviceSettingsControllerObserver
    : public InputDeviceSettingsController::Observer {
 public:
  void OnKeyboardConnected(const mojom::Keyboard& keyboard) override {
    num_keyboards_connected_++;
  }

  void OnKeyboardDisconnected(const mojom::Keyboard& keyboard) override {
    num_keyboards_connected_--;
  }

  uint32_t num_keyboards_connected() { return num_keyboards_connected_; }

 private:
  uint32_t num_keyboards_connected_;
};

class InputDeviceSettingsControllerTest : public AshTestBase {
 public:
  InputDeviceSettingsControllerTest() = default;
  InputDeviceSettingsControllerTest(const InputDeviceSettingsControllerTest&) =
      delete;
  InputDeviceSettingsControllerTest& operator=(
      const InputDeviceSettingsControllerTest&) = delete;
  ~InputDeviceSettingsControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kInputDeviceSettingsSplit);
    AshTestBase::SetUp();
    observer_ = std::make_unique<FakeInputDeviceSettingsControllerObserver>();
    std::unique_ptr<FakeKeyboardPrefHandler> keyboard_pref_handler =
        std::make_unique<FakeKeyboardPrefHandler>();
    keyboard_pref_handler_ = keyboard_pref_handler.get();
    controller()->AddObserver(observer_.get());
    controller()->SetPrefHandlersForTesting(std::move(keyboard_pref_handler));
    sample_keyboards_ = {kSampleKeyboardUsb, kSampleKeyboardInternal,
                         kSampleKeyboardBluetooth};
  }

  InputDeviceSettingsControllerImpl* controller() {
    return Shell::Get()->input_device_settings_controller();
  }

  void TearDown() override {
    controller()->RemoveObserver(observer_.get());
    observer_.reset();

    keyboard_pref_handler_ = nullptr;
    AshTestBase::TearDown();
  }

 protected:
  std::vector<ui::InputDevice> sample_keyboards_;
  std::unique_ptr<FakeInputDeviceSettingsControllerObserver> observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeKeyboardPrefHandler* keyboard_pref_handler_ = nullptr;
};

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingOne) {
  controller()->OnKeyboardListUpdated({kSampleKeyboardUsb}, {});

  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingMultiple) {
  controller()->OnKeyboardListUpdated(
      {kSampleKeyboardUsb, kSampleKeyboardInternal, kSampleKeyboardBluetooth},
      {});

  EXPECT_EQ(observer_->num_keyboards_connected(), 3u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 3u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingThenRemovingOne) {
  controller()->OnKeyboardListUpdated({kSampleKeyboardUsb}, {});

  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);

  controller()->OnKeyboardListUpdated({}, {(DeviceId)kSampleKeyboardUsb.id});

  EXPECT_EQ(observer_->num_keyboards_connected(), 0u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingThenRemovingMultiple) {
  controller()->OnKeyboardListUpdated(
      {kSampleKeyboardUsb, kSampleKeyboardInternal, kSampleKeyboardBluetooth},
      {});

  EXPECT_EQ(observer_->num_keyboards_connected(), 3u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 3u);

  controller()->OnKeyboardListUpdated(
      {},
      {(DeviceId)kSampleKeyboardUsb.id, (DeviceId)kSampleKeyboardInternal.id,
       (DeviceId)kSampleKeyboardBluetooth.id});

  EXPECT_EQ(observer_->num_keyboards_connected(), 0u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 3u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingAndRemoving) {
  controller()->OnKeyboardListUpdated({kSampleKeyboardUsb}, {});

  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);

  controller()->OnKeyboardListUpdated({kSampleKeyboardInternal},
                                      {(DeviceId)kSampleKeyboardUsb.id});

  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 2u);
}

TEST_F(InputDeviceSettingsControllerTest, DeletesPrefsWhenFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);

  const AccountId account_id = AccountId::FromUserEmail(kUserEmail);

  std::unique_ptr<TestingPrefServiceSimple> pref_service =
      std::make_unique<TestingPrefServiceSimple>();
  ash::RegisterUserProfilePrefs(pref_service->registry(), /*for_test=*/true);

  base::Value::Dict test_pref_value;
  test_pref_value.Set("Fake Key", base::Value::Dict());
  pref_service->SetDict(prefs::kKeyboardDeviceSettingsDictPref,
                        test_pref_value.Clone());
  pref_service->SetDict(prefs::kMouseDeviceSettingsDictPref,
                        test_pref_value.Clone());
  pref_service->SetDict(prefs::kPointingStickDeviceSettingsDictPref,
                        test_pref_value.Clone());
  pref_service->SetDict(prefs::kTouchpadDeviceSettingsDictPref,
                        test_pref_value.Clone());

  GetSessionControllerClient()->SetUserPrefService(account_id,
                                                   std::move(pref_service));
  SimulateUserLogin(account_id);

  PrefService* active_pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  EXPECT_EQ(base::Value::Dict(), active_pref_service->GetDict(
                                     prefs::kKeyboardDeviceSettingsDictPref));
  EXPECT_EQ(base::Value::Dict(),
            active_pref_service->GetDict(prefs::kMouseDeviceSettingsDictPref));
  EXPECT_EQ(base::Value::Dict(),
            active_pref_service->GetDict(
                prefs::kPointingStickDeviceSettingsDictPref));
  EXPECT_EQ(base::Value::Dict(), active_pref_service->GetDict(
                                     prefs::kTouchpadDeviceSettingsDictPref));
}

}  // namespace ash

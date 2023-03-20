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
#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/pointing_stick_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/functional.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/events/devices/device_data_manager_test_api.h"
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
const ui::InputDevice kSampleKeyboardUsb2 = {20, ui::INPUT_DEVICE_USB,
                                             "kSampleKeyboardUsb2"};

constexpr char kInitialUserEmail[] = "example2@abc.com";
constexpr char kUserEmail1[] = "example1@abc.com";
constexpr char kUserEmail2[] = "joy@abc.com";
}  // namespace

class FakeKeyboardPrefHandler : public KeyboardPrefHandler {
 public:
  void InitializeKeyboardSettings(PrefService* pref_service,
                                  mojom::Keyboard* keyboard) override {
    num_keyboard_settings_initialized_++;
  }
  void UpdateKeyboardSettings(PrefService* pref_service,
                              const mojom::Keyboard& keyboard) override {
    num_keyboard_settings_updated_++;
  }

  uint32_t num_keyboard_settings_initialized() {
    return num_keyboard_settings_initialized_;
  }

  uint32_t num_keyboard_settings_updated() {
    return num_keyboard_settings_updated_;
  }

 private:
  uint32_t num_keyboard_settings_initialized_ = 0;
  uint32_t num_keyboard_settings_updated_ = 0;
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

  void OnKeyboardSettingsUpdated(const mojom::Keyboard& keyboard) override {
    num_keyboards_settings_updated_++;
  }

  uint32_t num_keyboards_connected() { return num_keyboards_connected_; }
  uint32_t num_keyboards_settings_updated() {
    return num_keyboards_settings_updated_;
  }

 private:
  uint32_t num_keyboards_connected_;
  uint32_t num_keyboards_settings_updated_;
};

class InputDeviceSettingsControllerTest : public NoSessionAshTestBase {
 public:
  InputDeviceSettingsControllerTest() = default;
  InputDeviceSettingsControllerTest(const InputDeviceSettingsControllerTest&) =
      delete;
  InputDeviceSettingsControllerTest& operator=(
      const InputDeviceSettingsControllerTest&) = delete;
  ~InputDeviceSettingsControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();

    scoped_feature_list_.InitAndEnableFeature(
        features::kInputDeviceSettingsSplit);
    NoSessionAshTestBase::SetUp();

    // Resetter must be created before the controller is initialized.
    scoped_resetter_ = std::make_unique<
        InputDeviceSettingsController::ScopedResetterForTest>();

    observer_ = std::make_unique<FakeInputDeviceSettingsControllerObserver>();
    std::unique_ptr<FakeKeyboardPrefHandler> keyboard_pref_handler =
        std::make_unique<FakeKeyboardPrefHandler>();
    keyboard_pref_handler_ = keyboard_pref_handler.get();
    controller_ = std::make_unique<InputDeviceSettingsControllerImpl>(
        std::move(keyboard_pref_handler),
        std::make_unique<TouchpadPrefHandlerImpl>(),
        std::make_unique<MousePrefHandlerImpl>(),
        std::make_unique<PointingStickPrefHandlerImpl>(), task_runner_);
    controller_->AddObserver(observer_.get());
    sample_keyboards_ = {kSampleKeyboardUsb, kSampleKeyboardInternal,
                         kSampleKeyboardBluetooth};

    SimulateUserLogin(kInitialUserEmail);
  }

  void TearDown() override {
    observer_.reset();
    controller_.reset();
    keyboard_pref_handler_ = nullptr;

    // Scoped Resetter must be deleted before the test base is teared down.
    scoped_resetter_.reset();
    NoSessionAshTestBase::TearDown();

    task_runner_.reset();
  }

 protected:
  std::unique_ptr<InputDeviceSettingsControllerImpl> controller_;

  std::vector<ui::InputDevice> sample_keyboards_;
  std::unique_ptr<FakeInputDeviceSettingsControllerObserver> observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<InputDeviceSettingsController::ScopedResetterForTest>
      scoped_resetter_;
  FakeKeyboardPrefHandler* keyboard_pref_handler_ = nullptr;
};

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingOne) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingMultiple) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {kSampleKeyboardUsb, kSampleKeyboardInternal, kSampleKeyboardBluetooth});
  EXPECT_EQ(observer_->num_keyboards_connected(), 3u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 3u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingThenRemovingOne) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);

  ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
  EXPECT_EQ(observer_->num_keyboards_connected(), 0u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingThenRemovingMultiple) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {kSampleKeyboardUsb, kSampleKeyboardInternal, kSampleKeyboardBluetooth});
  EXPECT_EQ(observer_->num_keyboards_connected(), 3u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 3u);

  ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
  EXPECT_EQ(observer_->num_keyboards_connected(), 0u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 3u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingAndRemoving) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);

  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardInternal});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 2u);
}

TEST_F(InputDeviceSettingsControllerTest, DeletesPrefsWhenFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);

  const AccountId account_id = AccountId::FromUserEmail(kUserEmail1);
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

TEST_F(InputDeviceSettingsControllerTest,
       InitializeSettingsWhenUserSessionChanges) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);

  const AccountId account_id = AccountId::FromUserEmail(kUserEmail1);
  const AccountId account_id_2 = AccountId::FromUserEmail(kUserEmail2);

  SimulateUserLogin(account_id);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 2u);
  SimulateUserLogin(account_id_2);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 3u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardSettingsUpdated) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});

  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardUsb.id,
                                   mojom::KeyboardSettings::New());

  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 1u);
}

// Tests that given an invalid id, keyboard settings are not updated and
// observers are not notified.
TEST_F(InputDeviceSettingsControllerTest, KeyboardSettingsUpdatedInvalidId) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardUsb.id + 1,
                                   mojom::KeyboardSettings::New());

  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 0u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 0u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardSettingsUpdateMultiple) {
  // The SetKeyboardSettings call should update both keyboards since they have
  // the same |device_key|.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {kSampleKeyboardUsb, kSampleKeyboardUsb2});

  EXPECT_EQ(observer_->num_keyboards_connected(), 2u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 2u);
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardUsb.id,
                                   mojom::KeyboardSettings::New());
  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 2u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 1u);
}

}  // namespace ash

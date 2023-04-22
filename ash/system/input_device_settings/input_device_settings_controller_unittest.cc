// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler.h"
#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/pointing_stick_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/functional.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/known_user.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"

namespace ash {

using DeviceId = InputDeviceSettingsController::DeviceId;

namespace {
const ui::InputDevice kSampleKeyboardInternal = {5,
                                                 ui::INPUT_DEVICE_INTERNAL,
                                                 "kSampleKeyboardInternal",
                                                 "",
                                                 base::FilePath(),
                                                 0x1111,
                                                 0x1111,
                                                 0};
const ui::InputDevice kSampleKeyboardBluetooth = {
    10, ui::INPUT_DEVICE_BLUETOOTH, "kSampleKeyboardBluetooth"};
const ui::InputDevice kSampleKeyboardUsb = {15,
                                            ui::INPUT_DEVICE_USB,
                                            "kSampleKeyboardUsb",
                                            "",
                                            base::FilePath(),
                                            0x1111,
                                            0x2222,
                                            0};
const ui::InputDevice kSampleKeyboardUsb2 = {20,
                                             ui::INPUT_DEVICE_USB,
                                             "kSampleKeyboardUsb2",
                                             "",
                                             base::FilePath(),
                                             0x1111,
                                             0x3333,
                                             0};
const ui::InputDevice kSampleTouchpadInternal = {1,
                                                 ui::INPUT_DEVICE_INTERNAL,
                                                 "kSampleTouchpadInternal",
                                                 "",
                                                 base::FilePath(),
                                                 0x1111,
                                                 0x4444,
                                                 0};
const ui::InputDevice kSamplePointingStickInternal = {
    2, ui::INPUT_DEVICE_INTERNAL, "kSamplePointingStickInternal"};
const ui::InputDevice kSampleMouseInternal = {3, ui::INPUT_DEVICE_INTERNAL,
                                              "kSampleMouseInternal"};

constexpr char kUserEmail1[] = "example1@abc.com";
constexpr char kUserEmail2[] = "joy@abc.com";
const AccountId account_id_1 =
    AccountId::FromUserEmailGaiaId(kUserEmail1, kUserEmail1);
const AccountId account_id_2 =
    AccountId::FromUserEmailGaiaId(kUserEmail2, kUserEmail2);
}  // namespace

class FakeKeyboardPrefHandler : public KeyboardPrefHandler {
 public:
  void InitializeKeyboardSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) override {
    keyboard->settings = mojom::KeyboardSettings::New();
    num_keyboard_settings_initialized_++;
  }

  void InitializeLoginScreenKeyboardSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) override {
    num_login_screen_keyboard_settings_initialized_++;
  }

  void UpdateKeyboardSettings(PrefService* pref_service,
                              const mojom::KeyboardPolicies& keyboard_policies,
                              const mojom::Keyboard& keyboard) override {
    num_keyboard_settings_updated_++;
  }

  void UpdateLoginScreenKeyboardSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::KeyboardPolicies& keyboard_policies,
      const mojom::Keyboard& keyboard) override {
    num_login_screen_keyboard_settings_updated_++;
  }

  void InitializeWithDefaultKeyboardSettings(
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) override {
    num_initialize_default_keyboard_settings_calls_++;
  }

  uint32_t num_keyboard_settings_initialized() {
    return num_keyboard_settings_initialized_;
  }

  uint32_t num_keyboard_settings_updated() {
    return num_keyboard_settings_updated_;
  }

  uint32_t num_login_screen_keyboard_settings_initialized() {
    return num_login_screen_keyboard_settings_initialized_;
  }

  uint32_t num_login_screen_keyboard_settings_updated() {
    return num_login_screen_keyboard_settings_updated_;
  }

  uint32_t num_initialize_default_keyboard_settings_calls() {
    return num_initialize_default_keyboard_settings_calls_;
  }

  void reset_num_keyboard_settings_initialized() {
    num_keyboard_settings_initialized_ = 0;
  }

 private:
  uint32_t num_keyboard_settings_initialized_ = 0;
  uint32_t num_keyboard_settings_updated_ = 0;
  uint32_t num_login_screen_keyboard_settings_initialized_ = 0;
  uint32_t num_login_screen_keyboard_settings_updated_ = 0;
  uint32_t num_initialize_default_keyboard_settings_calls_ = 0;
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
        local_state(), std::move(keyboard_pref_handler),
        std::make_unique<TouchpadPrefHandlerImpl>(),
        std::make_unique<MousePrefHandlerImpl>(),
        std::make_unique<PointingStickPrefHandlerImpl>(), task_runner_);
    controller_->AddObserver(observer_.get());
    sample_keyboards_ = {kSampleKeyboardUsb, kSampleKeyboardInternal,
                         kSampleKeyboardBluetooth};

    TestSessionControllerClient* session_controller =
        GetSessionControllerClient();
    session_controller->Reset();

    user_manager::KnownUser::RegisterPrefs(local_state()->registry());
    auto user_1_prefs = std::make_unique<TestingPrefServiceSimple>();
    RegisterUserProfilePrefs(user_1_prefs->registry(), /*for_test=*/true);
    auto user_2_prefs = std::make_unique<TestingPrefServiceSimple>();
    RegisterUserProfilePrefs(user_2_prefs->registry(), /*for_test=*/true);
    session_controller->AddUserSession(kUserEmail1,
                                       user_manager::USER_TYPE_REGULAR,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(account_id_1,
                                           std::move(user_1_prefs));
    session_controller->AddUserSession(kUserEmail2,
                                       user_manager::USER_TYPE_REGULAR,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(account_id_2,
                                           std::move(user_2_prefs));

    session_controller->SwitchActiveUser(account_id_1);
    session_controller->SetSessionState(session_manager::SessionState::ACTIVE);
    // Reset the `num_keyboard_settings_initialized_` to account for the
    // `InitializeKeyboardSettings` call made after test setup where we
    // simualate
    //  a user logging in.
    keyboard_pref_handler_->reset_num_keyboard_settings_initialized();
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
  raw_ptr<FakeKeyboardPrefHandler, ExperimentalAsh> keyboard_pref_handler_ =
      nullptr;
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

  SimulateUserLogin(account_id_2);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 2u);
  SimulateUserLogin(account_id_1);
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

TEST_F(InputDeviceSettingsControllerTest, PrefsInitializedBasedOnLoginState) {
  ClearLogin();
  // Reset the `active_pref_service_` to account for the
  // `InitializeKeyboardSettings` call made after test setup where we simualate
  //  a user logging in.
  controller_->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetPrimaryUserPrefService());
  controller_->OnLoginScreenFocusedPodChanged(account_id_1);
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  EXPECT_EQ(
      keyboard_pref_handler_->num_login_screen_keyboard_settings_initialized(),
      1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 0u);
  controller_->OnLoginScreenFocusedPodChanged(account_id_2);
  EXPECT_EQ(
      keyboard_pref_handler_->num_login_screen_keyboard_settings_initialized(),
      2u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 0u);
  SimulateUserLogin(account_id_1);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
  EXPECT_EQ(
      keyboard_pref_handler_->num_login_screen_keyboard_settings_initialized(),
      2u);
}

TEST_F(InputDeviceSettingsControllerTest, UpdateLoginScreenSettings) {
  controller_->OnLoginScreenFocusedPodChanged(account_id_1);
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardUsb.id,
                                   mojom::KeyboardSettings::New());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 1u);
  // Expect multiple calls to persist settings to the login screen prefs due to:
  // updates to the following: active pref service, keyboard settings,
  // list of keyboards.
  EXPECT_EQ(
      keyboard_pref_handler_->num_login_screen_keyboard_settings_updated(), 3u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardSettingsAreValid) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardInternal});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
  const mojom::KeyboardSettingsPtr settings = mojom::KeyboardSettings::New();
  settings->suppress_meta_fkey_rewrites = !kDefaultSuppressMetaFKeyRewrites;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings.Clone());

  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 0u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 0u);

  // The keyboard is internal and it doesn't have capslock remapping.
  settings->suppress_meta_fkey_rewrites = kDefaultSuppressMetaFKeyRewrites;
  settings->modifier_remappings[ui::mojom::ModifierKey::kCapsLock] =
      ui::mojom::ModifierKey::kAlt;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings.Clone());

  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 0u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 0u);
}

TEST_F(InputDeviceSettingsControllerTest,
       RecordSetKeyboardSetttingsValidMetric) {
  base::HistogramTester histogram_tester;
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardInternal});
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   mojom::KeyboardSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Keyboard.SetSettingsSucceeded", true,
      /*expected_count=*/1u);

  // Set keyboard with invalid settings.
  const mojom::KeyboardSettingsPtr settings = mojom::KeyboardSettings::New();
  settings->suppress_meta_fkey_rewrites = !kDefaultSuppressMetaFKeyRewrites;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings.Clone());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Keyboard.SetSettingsSucceeded", false,
      /*expected_count=*/1u);
}

TEST_F(InputDeviceSettingsControllerTest,
       RecordSetTouchpadSetttingsValidMetric) {
  base::HistogramTester histogram_tester;
  ui::DeviceDataManagerTestApi().SetTouchpadDevices({kSampleTouchpadInternal});
  controller_->SetTouchpadSettings((DeviceId)kSampleTouchpadInternal.id,
                                   mojom::TouchpadSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Touchpad.SetSettingsSucceeded", true,
      /*expected_count=*/1u);

  // Set touchpad with invalid id.
  controller_->SetTouchpadSettings(/*id=*/4, mojom::TouchpadSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Touchpad.SetSettingsSucceeded", false,
      /*expected_count=*/1u);
}

TEST_F(InputDeviceSettingsControllerTest,
       RecordSetPointingStickSetttingsValidMetric) {
  base::HistogramTester histogram_tester;
  ui::DeviceDataManagerTestApi().SetPointingStickDevices(
      {kSamplePointingStickInternal});
  controller_->SetPointingStickSettings(
      (DeviceId)kSamplePointingStickInternal.id,
      mojom::PointingStickSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.PointingStick.SetSettingsSucceeded", true,
      /*expected_count=*/1u);

  // Set pointing stick with invalid id.
  controller_->SetPointingStickSettings(/*id=*/4,
                                        mojom::PointingStickSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.PointingStick.SetSettingsSucceeded", false,
      /*expected_count=*/1u);
}

TEST_F(InputDeviceSettingsControllerTest, RecordSetMouseSetttingsValidMetric) {
  base::HistogramTester histogram_tester;
  ui::DeviceDataManagerTestApi().SetMouseDevices({kSampleMouseInternal});
  controller_->SetMouseSettings((DeviceId)kSampleMouseInternal.id,
                                mojom::MouseSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Mouse.SetSettingsSucceeded", true,
      /*expected_count=*/1u);

  // Set mouse with invalid id.
  controller_->SetMouseSettings(/*id=*/4, mojom::MouseSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Mouse.SetSettingsSucceeded", false,
      /*expected_count=*/1u);
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
  ui::InputDevice sample_usb_keyboard_copy = kSampleKeyboardUsb;
  sample_usb_keyboard_copy.id = kSampleKeyboardUsb2.id;
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {kSampleKeyboardUsb, sample_usb_keyboard_copy});

  EXPECT_EQ(observer_->num_keyboards_connected(), 2u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 2u);
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardUsb.id,
                                   mojom::KeyboardSettings::New());
  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 2u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 1u);
}

TEST_F(InputDeviceSettingsControllerTest, RecordsMetricsSettings) {
  // Initially expect no user preferences recorded.
  base::HistogramTester histogram_tester;
  controller_->OnKeyboardListUpdated({kSampleKeyboardUsb, kSampleKeyboardUsb2},
                                     {});
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.TopRowAreFKeys."
      "Initial",
      /*expected_count=*/2u);
  SimulateUserLogin(account_id_2);
  task_runner_->RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.TopRowAreFKeys."
      "Initial",
      /*expected_count=*/4u);

  // Test Metrics Updates when setKeyboardSettings is called.
  auto updated_settings = mojom::KeyboardSettings::New();
  updated_settings.get()->top_row_are_fkeys = true;
  controller_->SetKeyboardSettings(kSampleKeyboardUsb.id,
                                   std::move(updated_settings));
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.TopRowAreFKeys."
      "Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS."
      "BlockMetaFKeyRewrites.Changed",
      /*expected_count=*/0);

  // Test Metrics Updates when setTouchpadSettings is called.
  controller_->OnTouchpadListUpdated({kSampleTouchpadInternal}, {});
  controller_->SetTouchpadSettings(kSampleTouchpadInternal.id,
                                   mojom::TouchpadSettings::New());
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.Internal.AccelerationEnabled.Changed",
      /*expected_count=*/1u);
}

TEST_F(InputDeviceSettingsControllerTest, GetGeneralizedTopRowAreFKeys) {
  // If there no keyboards, return false.
  EXPECT_EQ(false, controller_->GetGeneralizedTopRowAreFKeys());

  // If there is only internal keyboard, return its top_row_are_fkeys value.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardInternal});

  auto internal_keyboard_settings = mojom::KeyboardSettings::New();
  internal_keyboard_settings->top_row_are_fkeys = true;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   internal_keyboard_settings.Clone());
  EXPECT_EQ(true, controller_->GetGeneralizedTopRowAreFKeys());

  // If there are multiple external keyboards, return the top_row_are_fkeys
  // value of the external keyboard which has the largest device id.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {kSampleKeyboardInternal, kSampleKeyboardUsb, kSampleKeyboardUsb2});

  auto settings = mojom::KeyboardSettings::New();
  settings->top_row_are_fkeys = true;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardUsb.id,
                                   settings.Clone());
  EXPECT_EQ(false, controller_->GetGeneralizedTopRowAreFKeys());
}

TEST_F(InputDeviceSettingsControllerTest,
       KeyboardSettingsAreValidWithEnterprisePolicy) {
  // Test when top_row_are_fkeys_policy doesn't exist.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardInternal});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
  const mojom::KeyboardSettingsPtr settings = mojom::KeyboardSettings::New();
  settings->top_row_are_fkeys = !kDefaultTopRowAreFKeys;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings.Clone());
  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 1u);

  // Test when policy status is kRecommended.
  std::unique_ptr<TestingPrefServiceSimple> pref_service =
      std::make_unique<TestingPrefServiceSimple>();
  ash::RegisterUserProfilePrefs(pref_service->registry(), /*for_test=*/true);
  controller_->OnActiveUserPrefServiceChanged(pref_service.get());
  pref_service->SetRecommendedPref(prefs::kSendFunctionKeys,
                                   base::Value(kDefaultTopRowAreFKeys));
  settings->top_row_are_fkeys = !kDefaultTopRowAreFKeys;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings.Clone());
  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 2u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 2u);

  // Test when policy status is kManaged and the settings are valid.
  pref_service->SetManagedPref(prefs::kSendFunctionKeys,
                               base::Value(kDefaultTopRowAreFKeys));
  settings->top_row_are_fkeys = kDefaultTopRowAreFKeys;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings.Clone());
  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 3u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 3u);

  // Test when policy status is kManaged and the settings are invalid.
  pref_service->SetManagedPref(prefs::kSendFunctionKeys,
                               base::Value(!kDefaultTopRowAreFKeys));
  settings->top_row_are_fkeys = kDefaultTopRowAreFKeys;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings.Clone());
  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 3u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 3u);
  controller_->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
}

}  // namespace ash

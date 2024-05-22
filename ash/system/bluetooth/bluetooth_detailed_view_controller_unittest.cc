// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_controller.h"

#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/fake_hats_bluetooth_revamp_trigger_impl.h"
#include "ash/public/cpp/hats_bluetooth_revamp_trigger.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/bluetooth/bluetooth_device_list_controller.h"
#include "ash/system/bluetooth/fake_bluetooth_detailed_view.h"
#include "ash/system/bluetooth/fake_bluetooth_device_list_controller.h"
#include "ash/system/bluetooth/hid_preserving_controller/hid_preserving_bluetooth_state_controller_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_cache.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_operation_handler.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/ash/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"
#include "mojo/public/cpp/bindings/clone_traits.h"

namespace ash {

namespace {

using bluetooth_config::FakeDeviceOperationHandler;
using bluetooth_config::ScopedBluetoothConfigTestHelper;
using bluetooth_config::mojom::AudioOutputCapability;
using bluetooth_config::mojom::BluetoothDeviceProperties;
using bluetooth_config::mojom::BluetoothSystemState;
using bluetooth_config::mojom::DeviceConnectionState;
using bluetooth_config::mojom::PairedBluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

const char kDeviceId[] = "/device/id";

class FakeBluetoothDetailedViewFactory : public BluetoothDetailedView::Factory {
 public:
  FakeBluetoothDetailedViewFactory() = default;
  FakeBluetoothDetailedViewFactory(const FakeBluetoothDetailedViewFactory&) =
      delete;
  const FakeBluetoothDetailedViewFactory& operator=(
      const FakeBluetoothDetailedViewFactory&) = delete;
  ~FakeBluetoothDetailedViewFactory() override = default;

  FakeBluetoothDetailedView* bluetooth_detailed_view() {
    return bluetooth_detailed_view_;
  }

 private:
  std::unique_ptr<BluetoothDetailedView> CreateForTesting(
      BluetoothDetailedView::Delegate* delegate) override {
    DCHECK(!bluetooth_detailed_view_);
    std::unique_ptr<FakeBluetoothDetailedView> bluetooth_detailed_view =
        std::make_unique<FakeBluetoothDetailedView>(delegate);
    bluetooth_detailed_view_ = bluetooth_detailed_view.get();
    return bluetooth_detailed_view;
  }

  raw_ptr<FakeBluetoothDetailedView, DanglingUntriaged>
      bluetooth_detailed_view_ = nullptr;
};

class FakeBluetoothDeviceListControllerFactory
    : public BluetoothDeviceListController::Factory {
 public:
  FakeBluetoothDeviceListControllerFactory() = default;
  FakeBluetoothDeviceListControllerFactory(
      const FakeBluetoothDeviceListControllerFactory&) = delete;
  const FakeBluetoothDeviceListControllerFactory& operator=(
      const FakeBluetoothDeviceListControllerFactory&) = delete;
  ~FakeBluetoothDeviceListControllerFactory() override = default;

  FakeBluetoothDeviceListController* bluetooth_device_list_controller() {
    return bluetooth_device_list_controller_;
  }

 private:
  std::unique_ptr<BluetoothDeviceListController> CreateForTesting() override {
    DCHECK(!bluetooth_device_list_controller_);
    std::unique_ptr<FakeBluetoothDeviceListController>
        bluetooth_device_list_controller =
            std::make_unique<FakeBluetoothDeviceListController>();
    bluetooth_device_list_controller_ = bluetooth_device_list_controller.get();
    return bluetooth_device_list_controller;
  }

  raw_ptr<FakeBluetoothDeviceListController, DanglingUntriaged>
      bluetooth_device_list_controller_ = nullptr;
};

}  // namespace

class BluetoothDetailedViewControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    GetPrimaryUnifiedSystemTray()->ShowBubble();

    BluetoothDetailedView::Factory::SetFactoryForTesting(
        &bluetooth_detailed_view_factory_);
    BluetoothDeviceListController::Factory::SetFactoryForTesting(
        &bluetooth_device_list_controller_factory_);

    GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller()
        ->ShowBluetoothDetailedView();

    fake_trigger_impl_ = std::make_unique<FakeHatsBluetoothRevampTriggerImpl>();

    bluetooth_detailed_view_controller_ =
        static_cast<BluetoothDetailedViewController*>(
            GetPrimaryUnifiedSystemTray()
                ->bubble()
                ->unified_system_tray_controller()
                ->detailed_view_controller());

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    BluetoothDeviceListController::Factory::SetFactoryForTesting(nullptr);
    BluetoothDetailedView::Factory::SetFactoryForTesting(nullptr);

    AshTestBase::TearDown();
  }

  BluetoothSystemState GetBluetoothAdapterState() {
    return bluetooth_config_test_helper()
        ->fake_adapter_state_controller()
        ->GetAdapterState();
  }

  PairedBluetoothDevicePropertiesPtr CreatePairedDevice(
      DeviceConnectionState connection_state) {
    PairedBluetoothDevicePropertiesPtr paired_properties =
        PairedBluetoothDeviceProperties::New();
    paired_properties->device_properties = BluetoothDeviceProperties::New();
    paired_properties->device_properties->connection_state = connection_state;
    return paired_properties;
  }

  void SetPairedDevices(
      std::vector<PairedBluetoothDevicePropertiesPtr> paired_devices) {
    bluetooth_config_test_helper()->fake_device_cache()->SetPairedDevices(
        std::move(paired_devices));
    base::RunLoop().RunUntilIdle();
  }

  void SetBluetoothAdapterState(BluetoothSystemState system_state) {
    bluetooth_config_test_helper()
        ->fake_adapter_state_controller()
        ->SetSystemState(system_state);
    base::RunLoop().RunUntilIdle();
  }

  BluetoothDetailedView::Delegate* bluetooth_detailed_view_delegate() {
    return bluetooth_detailed_view_controller_;
  }

  FakeBluetoothDetailedView* bluetooth_detailed_view() {
    return bluetooth_detailed_view_factory_.bluetooth_detailed_view();
  }

  FakeBluetoothDeviceListController* bluetooth_device_list_controller() {
    return bluetooth_device_list_controller_factory_
        .bluetooth_device_list_controller();
  }

  FakeDeviceOperationHandler* fake_device_operation_handler() {
    return bluetooth_config_test_helper()->fake_device_operation_handler();
  }

  size_t GetTryToShowSurveyCount() {
    return fake_trigger_impl_->try_to_show_survey_count();
  }

 private:
  ScopedBluetoothConfigTestHelper* bluetooth_config_test_helper() {
    return ash_test_helper()->bluetooth_config_test_helper();
  }

  std::unique_ptr<FakeHatsBluetoothRevampTriggerImpl> fake_trigger_impl_;
  raw_ptr<BluetoothDetailedViewController, DanglingUntriaged>
      bluetooth_detailed_view_controller_;
  FakeBluetoothDetailedViewFactory bluetooth_detailed_view_factory_;
  FakeBluetoothDeviceListControllerFactory
      bluetooth_device_list_controller_factory_;
};

TEST_F(BluetoothDetailedViewControllerTest,
       TransitionToMainViewWhenBluetoothUnavailable) {
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()
                  ->bubble()
                  ->unified_system_tray_controller()
                  ->IsDetailedViewShown());

  SetBluetoothAdapterState(BluetoothSystemState::kUnavailable);

  EXPECT_FALSE(GetPrimaryUnifiedSystemTray()
                   ->bubble()
                   ->unified_system_tray_controller()
                   ->IsDetailedViewShown());
}

TEST_F(BluetoothDetailedViewControllerTest,
       NotifiesWhenBluetoothEnabledStateChanges) {
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  EXPECT_TRUE(
      bluetooth_detailed_view()->last_bluetooth_enabled_state().has_value());
  EXPECT_TRUE(
      bluetooth_detailed_view()->last_bluetooth_enabled_state().value());
  EXPECT_TRUE(bluetooth_device_list_controller()
                  ->last_bluetooth_enabled_state()
                  .has_value());
  EXPECT_TRUE(bluetooth_device_list_controller()
                  ->last_bluetooth_enabled_state()
                  .value());

  SetBluetoothAdapterState(BluetoothSystemState::kDisabled);
  EXPECT_FALSE(
      bluetooth_detailed_view()->last_bluetooth_enabled_state().value());
  EXPECT_FALSE(bluetooth_device_list_controller()
                   ->last_bluetooth_enabled_state()
                   .value());

  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_TRUE(
      bluetooth_detailed_view()->last_bluetooth_enabled_state().value());
  EXPECT_TRUE(bluetooth_device_list_controller()
                  ->last_bluetooth_enabled_state()
                  .value());
}

TEST_F(BluetoothDetailedViewControllerTest,
       ChangesBluetoothEnabledStateWhenTogglePressed) {
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  EXPECT_EQ(0u, GetTryToShowSurveyCount());

  bluetooth_detailed_view_delegate()->OnToggleClicked(/*new_state=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
  EXPECT_EQ(1u, GetTryToShowSurveyCount());

  bluetooth_detailed_view_delegate()->OnToggleClicked(/*new_state=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothSystemState::kEnabling, GetBluetoothAdapterState());
  EXPECT_EQ(2u, GetTryToShowSurveyCount());
}

TEST_F(BluetoothDetailedViewControllerTest,
       OnPairNewDeviceRequestedOpensBluetoothDialogWithHatsTrigger) {
  EXPECT_EQ(0u, GetTryToShowSurveyCount());
  EXPECT_EQ(0, GetSystemTrayClient()->show_bluetooth_pairing_dialog_count());
  bluetooth_detailed_view_delegate()->OnPairNewDeviceRequested();
  EXPECT_EQ(1, GetSystemTrayClient()->show_bluetooth_pairing_dialog_count());
  EXPECT_EQ(1u, GetTryToShowSurveyCount());
}

TEST_F(BluetoothDetailedViewControllerTest,
       OnDeviceListAudioCapableItemSelected) {
  PairedBluetoothDevicePropertiesPtr selected_device =
      CreatePairedDevice(DeviceConnectionState::kNotConnected);
  selected_device->device_properties->id = kDeviceId;
  selected_device->device_properties->audio_capability =
      AudioOutputCapability::kCapableOfAudioOutput;

  std::vector<PairedBluetoothDevicePropertiesPtr> paired_devices;
  paired_devices.push_back(mojo::Clone(selected_device));
  SetPairedDevices(std::move(paired_devices));

  EXPECT_EQ(0, GetSystemTrayClient()->show_bluetooth_settings_count());
  EXPECT_TRUE(
      GetSystemTrayClient()->last_bluetooth_settings_device_id().empty());
  EXPECT_EQ(0u, fake_device_operation_handler()->perform_connect_call_count());

  bluetooth_detailed_view_delegate()->OnDeviceListItemSelected(selected_device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetSystemTrayClient()->show_bluetooth_settings_count());
  EXPECT_TRUE(
      GetSystemTrayClient()->last_bluetooth_settings_device_id().empty());
  EXPECT_EQ(1u, fake_device_operation_handler()->perform_connect_call_count());
  EXPECT_STREQ(kDeviceId, fake_device_operation_handler()
                              ->last_perform_connect_device_id()
                              .c_str());

  selected_device->device_properties->connection_state =
      DeviceConnectionState::kConnected;
  bluetooth_detailed_view_delegate()->OnDeviceListItemSelected(selected_device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, GetSystemTrayClient()->show_bluetooth_settings_count());
  EXPECT_STREQ(
      kDeviceId,
      GetSystemTrayClient()->last_bluetooth_settings_device_id().c_str());
}

TEST_F(BluetoothDetailedViewControllerTest,
       OnDeviceListNonAudioCapableItemSelected) {
  PairedBluetoothDevicePropertiesPtr selected_device =
      CreatePairedDevice(DeviceConnectionState::kNotConnected);
  selected_device->device_properties->id = kDeviceId;

  EXPECT_EQ(0, GetSystemTrayClient()->show_bluetooth_settings_count());
  EXPECT_TRUE(
      GetSystemTrayClient()->last_bluetooth_settings_device_id().empty());

  bluetooth_detailed_view_delegate()->OnDeviceListItemSelected(selected_device);

  EXPECT_EQ(1, GetSystemTrayClient()->show_bluetooth_settings_count());
  EXPECT_STREQ(
      kDeviceId,
      GetSystemTrayClient()->last_bluetooth_settings_device_id().c_str());
  EXPECT_EQ(0u, fake_device_operation_handler()->perform_connect_call_count());
}

TEST_F(BluetoothDetailedViewControllerTest,
       CorrectlySplitsDevicesByConnectionState) {
  std::vector<PairedBluetoothDevicePropertiesPtr> paired_devices;
  paired_devices.push_back(
      CreatePairedDevice(DeviceConnectionState::kNotConnected));
  paired_devices.push_back(
      CreatePairedDevice(DeviceConnectionState::kConnecting));
  paired_devices.push_back(
      CreatePairedDevice(DeviceConnectionState::kConnected));

  EXPECT_EQ(0u, bluetooth_device_list_controller()->connected_devices_count());
  EXPECT_EQ(
      0u,
      bluetooth_device_list_controller()->previously_connected_devices_count());

  SetPairedDevices(std::move(paired_devices));

  EXPECT_EQ(1u, bluetooth_device_list_controller()->connected_devices_count());
  EXPECT_EQ(
      2u,
      bluetooth_device_list_controller()->previously_connected_devices_count());
}

class BluetoothDetailedViewControllerConnectWarningTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        features::kBluetoothDisconnectWarning);

    GetPrimaryUnifiedSystemTray()->ShowBubble();

    hid_preserving_bluetooth_state_test_helper_ =
        std::make_unique<HidPreservingBluetoothStateControllerTestHelper>();

    hid_preserving_bluetooth_state_test_helper_->fake_hid_preserving_bluetooth()
        ->SetScopedBluetoothConfigHelper(bluetooth_config_test_helper());

    BluetoothDetailedView::Factory::SetFactoryForTesting(
        &bluetooth_detailed_view_factory_);
    BluetoothDeviceListController::Factory::SetFactoryForTesting(
        &bluetooth_device_list_controller_factory_);

    GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller()
        ->ShowBluetoothDetailedView();

    fake_trigger_impl_ = std::make_unique<FakeHatsBluetoothRevampTriggerImpl>();

    bluetooth_detailed_view_controller_ =
        static_cast<BluetoothDetailedViewController*>(
            GetPrimaryUnifiedSystemTray()
                ->bubble()
                ->unified_system_tray_controller()
                ->detailed_view_controller());

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    BluetoothDeviceListController::Factory::SetFactoryForTesting(nullptr);
    BluetoothDetailedView::Factory::SetFactoryForTesting(nullptr);

    AshTestBase::TearDown();
  }

  BluetoothSystemState GetBluetoothAdapterState() {
    return bluetooth_config_test_helper()
        ->fake_adapter_state_controller()
        ->GetAdapterState();
  }

  void SetBluetoothAdapterState(BluetoothSystemState system_state) {
    bluetooth_config_test_helper()
        ->fake_adapter_state_controller()
        ->SetSystemState(system_state);
    base::RunLoop().RunUntilIdle();
  }

  BluetoothDetailedView::Delegate* bluetooth_detailed_view_delegate() {
    return bluetooth_detailed_view_controller_;
  }

  size_t GetTryToShowSurveyCount() {
    return fake_trigger_impl_->try_to_show_survey_count();
  }

  void SetShouldShowWarningDialog(bool should_show_warning_dialog) {
    hid_preserving_bluetooth_state_test_helper_->fake_hid_preserving_bluetooth()
        ->SetShouldShowWarningDialog(should_show_warning_dialog);
  }

  void CompleteShowWarningDialog(bool show_dialog_result) {
    hid_preserving_bluetooth_state_test_helper_->fake_hid_preserving_bluetooth()
        ->CompleteShowDialog(show_dialog_result);
  }

  size_t GetDialogShownCount() {
    return hid_preserving_bluetooth_state_test_helper_
        ->fake_hid_preserving_bluetooth()
        ->dialog_shown_count();
  }

 private:
  ScopedBluetoothConfigTestHelper* bluetooth_config_test_helper() {
    return ash_test_helper()->bluetooth_config_test_helper();
  }

  std::unique_ptr<FakeHatsBluetoothRevampTriggerImpl> fake_trigger_impl_;
  raw_ptr<BluetoothDetailedViewController, DanglingUntriaged>
      bluetooth_detailed_view_controller_;
  FakeBluetoothDetailedViewFactory bluetooth_detailed_view_factory_;
  FakeBluetoothDeviceListControllerFactory
      bluetooth_device_list_controller_factory_;

  std::unique_ptr<HidPreservingBluetoothStateControllerTestHelper>
      hid_preserving_bluetooth_state_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BluetoothDetailedViewControllerConnectWarningTest,
       ChangesBluetoothEnabledStateWhenTogglePressed) {
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  EXPECT_EQ(0u, GetTryToShowSurveyCount());
  EXPECT_EQ(0u, GetDialogShownCount());

  bluetooth_detailed_view_delegate()->OnToggleClicked(/*new_state=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
  EXPECT_EQ(1u, GetTryToShowSurveyCount());
  EXPECT_EQ(0u, GetDialogShownCount());

  bluetooth_detailed_view_delegate()->OnToggleClicked(/*new_state=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothSystemState::kEnabling, GetBluetoothAdapterState());
  EXPECT_EQ(2u, GetTryToShowSurveyCount());
  EXPECT_EQ(0u, GetDialogShownCount());
}

TEST_F(BluetoothDetailedViewControllerConnectWarningTest,
       SimulateShowWarningDialogOnDisconnect_ResultTrue) {
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  EXPECT_EQ(0u, GetTryToShowSurveyCount());
  EXPECT_EQ(0u, GetDialogShownCount());

  SetShouldShowWarningDialog(true);
  bluetooth_detailed_view_delegate()->OnToggleClicked(/*new_state=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  EXPECT_EQ(1u, GetTryToShowSurveyCount());
  EXPECT_EQ(1u, GetDialogShownCount());

  CompleteShowWarningDialog(/*show_dialog_result=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
  EXPECT_EQ(1u, GetTryToShowSurveyCount());
  EXPECT_EQ(1u, GetDialogShownCount());
}

TEST_F(BluetoothDetailedViewControllerConnectWarningTest,
       SimulateShowWarningDialogOnDisconnect_ResultFalse) {
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  EXPECT_EQ(0u, GetTryToShowSurveyCount());
  EXPECT_EQ(0u, GetDialogShownCount());

  SetShouldShowWarningDialog(true);
  bluetooth_detailed_view_delegate()->OnToggleClicked(/*new_state=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  EXPECT_EQ(1u, GetTryToShowSurveyCount());
  EXPECT_EQ(1u, GetDialogShownCount());

  CompleteShowWarningDialog(/*show_dialog_result=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  EXPECT_EQ(1u, GetTryToShowSurveyCount());
  EXPECT_EQ(1u, GetDialogShownCount());
}

}  // namespace ash

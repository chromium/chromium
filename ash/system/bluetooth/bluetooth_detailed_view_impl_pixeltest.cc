// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/system/bluetooth/bluetooth_detailed_view_impl.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_cache.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-shared.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/ash/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {
namespace {

using bluetooth_config::ScopedBluetoothConfigTestHelper;
using bluetooth_config::mojom::BluetoothDeviceProperties;
using bluetooth_config::mojom::BluetoothSystemState;
using bluetooth_config::mojom::DeviceConnectionState;
using bluetooth_config::mojom::PairedBluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

// Creates a paired Bluetooth device.
PairedBluetoothDevicePropertiesPtr CreatePairedDevice(
    DeviceConnectionState connection_state,
    const std::u16string& public_name) {
  PairedBluetoothDevicePropertiesPtr paired_properties =
      PairedBluetoothDeviceProperties::New();
  paired_properties->device_properties = BluetoothDeviceProperties::New();
  paired_properties->device_properties->connection_state = connection_state;
  paired_properties->device_properties->public_name = public_name;
  return paired_properties;
}

// Returns appropriate screenshot suffix based on whether the feature flag is
// enabled.
std::string GetScreenshotName(const std::string& test_name, bool enabled) {
  return test_name + (enabled ? "_unavailable_state_enabled"
                              : "_unavailable_state_disabled");
}

// Pixel tests for the quick settings Bluetooth detailed view.
class BluetoothDetailedViewImplPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  BluetoothDetailedViewImplPixelTest() {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatureState(
        chromeos::features::kBluetoothWifiQSPodRefresh,
        IsBluetoothWifiQSPodRefreshEnabled());
  }

  bool IsBluetoothWifiQSPodRefreshEnabled() { return GetParam(); }

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  // Sets the list of paired devices in the device cache.
  void SetPairedDevices(
      std::vector<PairedBluetoothDevicePropertiesPtr> paired_devices) {
    ash_test_helper()
        ->bluetooth_config_test_helper()
        ->fake_device_cache()
        ->SetPairedDevices(std::move(paired_devices));
  }

  void SetBluetoothSystemState(BluetoothSystemState system_state) {
    ash_test_helper()
        ->bluetooth_config_test_helper()
        ->fake_adapter_state_controller()
        ->SetSystemState(system_state);
    base::RunLoop().RunUntilIdle();
  }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BluetoothDetailedViewImplPixelTest,
    /*IsBluetoothWifiQSPodRefreshEnabled()=*/testing::Bool());

TEST_P(BluetoothDetailedViewImplPixelTest, Basics) {
  // Create test devices.
  std::vector<PairedBluetoothDevicePropertiesPtr> paired_devices;
  paired_devices.push_back(
      CreatePairedDevice(DeviceConnectionState::kConnected, u"Keyboard"));
  paired_devices.push_back(
      CreatePairedDevice(DeviceConnectionState::kNotConnected, u"Mouse"));
  SetPairedDevices(std::move(paired_devices));

  // Show the system tray bubble.
  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());

  // Show the Bluetooth detailed view.
  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowBluetoothDetailedView();
  TrayDetailedView* detailed_view =
      system_tray->bubble()
          ->quick_settings_view()
          ->GetDetailedViewForTest<TrayDetailedView>();
  ASSERT_TRUE(detailed_view);

  // Compare pixels.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GetScreenshotName("check_view", IsBluetoothWifiQSPodRefreshEnabled()),
      /*revision_number=*/9, detailed_view));
}

TEST_P(BluetoothDetailedViewImplPixelTest, BluetoothUnavailable) {
  if (!IsBluetoothWifiQSPodRefreshEnabled()) {
    GTEST_SKIP() << "If Bluetooth is unavailable the BluetoothDetailedView is "
                    "not accessible";
  }
  SetBluetoothSystemState(BluetoothSystemState::kUnavailable);

  // Show the system tray bubble.
  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());

  // Show the Bluetooth detailed view.
  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowBluetoothDetailedView();
  TrayDetailedView* detailed_view =
      system_tray->bubble()
          ->quick_settings_view()
          ->GetDetailedViewForTest<TrayDetailedView>();
  ASSERT_TRUE(detailed_view);

  // Compare pixels.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GetScreenshotName("bluetooth_unavailable_view",
                        IsBluetoothWifiQSPodRefreshEnabled()),
      /*revision_number=*/0, detailed_view));
}

}  // namespace
}  // namespace ash

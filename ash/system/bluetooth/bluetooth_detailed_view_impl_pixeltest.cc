// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
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
#include "chromeos/ash/services/bluetooth_config/fake_device_cache.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/ash/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {
namespace {

using bluetooth_config::ScopedBluetoothConfigTestHelper;
using bluetooth_config::mojom::BluetoothDeviceProperties;
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

// Pixel tests for the quick settings Bluetooth detailed view.
class BluetoothDetailedViewImplPixelTest : public AshTestBase {
 public:
  BluetoothDetailedViewImplPixelTest() {
    feature_list_.InitWithFeatures(
        {features::kQsRevamp, chromeos::features::kJelly}, {});
  }

  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BluetoothDetailedViewImplPixelTest, Basics) {
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
      system_tray->bubble()->quick_settings_view()->GetDetailedViewForTest();
  ASSERT_TRUE(detailed_view);

  // Compare pixels.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_view",
      /*revision_number=*/2, detailed_view));
}

}  // namespace
}  // namespace ash

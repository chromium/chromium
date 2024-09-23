// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_FEATURE_POD_CONTROLLER_H_

#include <optional>
#include <string>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of the feature tile that allows users to toggle whether Bluetooth
// is enabled or disabled, and that allows users to navigate to a more detailed
// page with a Bluetooth device list.
class ASH_EXPORT BluetoothFeaturePodController
    : public FeaturePodControllerBase,
      public bluetooth_config::mojom::SystemPropertiesObserver {
 public:
  explicit BluetoothFeaturePodController(
      UnifiedSystemTrayController* tray_controller);
  BluetoothFeaturePodController(const BluetoothFeaturePodController&) = delete;
  BluetoothFeaturePodController& operator=(
      const BluetoothFeaturePodController&) = delete;
  ~BluetoothFeaturePodController() override;

  // FeaturePodControllerBase:
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;

 private:
  // Helper struct to organize the cached information of a connected device.
  struct BluetoothDeviceNameAndBatteryInfo {
    BluetoothDeviceNameAndBatteryInfo(
        const std::u16string& device_name,
        bluetooth_config::mojom::DeviceBatteryInfoPtr battery_info);
    ~BluetoothDeviceNameAndBatteryInfo();

    const std::u16string device_name;
    const bluetooth_config::mojom::DeviceBatteryInfoPtr battery_info;
  };

  bool DoesFirstConnectedDeviceHaveBatteryInfo() const;
  int GetFirstConnectedDeviceBatteryLevelForDisplay() const;

  const gfx::VectorIcon& ComputeButtonIcon() const;
  std::u16string ComputeButtonLabel() const;
  std::u16string ComputeButtonSubLabel() const;
  std::u16string ComputeTooltip() const;

  // Returns true if `tile_` is enabled, otherwise false.
  bool IsButtonEnabled() const;

  // Returns true if `tile_` is toggled, otherwise false.
  bool IsButtonToggled() const;

  // Returns true if Bluetooth is available, otherwise false.
  bool IsBluetoothAvailable() const;

  // Updates `tile_` state to reflect the cached Bluetooth state.
  void UpdateTileStateIfExists();

  // bluetooth_config::mojom::SystemPropertiesObserver
  void OnPropertiesUpdated(bluetooth_config::mojom::BluetoothSystemPropertiesPtr
                               properties) override;

  mojo::Remote<bluetooth_config::mojom::CrosBluetoothConfig>
      remote_cros_bluetooth_config_;
  mojo::Receiver<bluetooth_config::mojom::SystemPropertiesObserver>
      cros_system_properties_observer_receiver_{this};

  mojo::Remote<mojom::HidPreservingBluetoothStateController>
      remote_hid_preserving_bluetooth_;

  size_t connected_device_count_ = 0;
  std::optional<BluetoothDeviceNameAndBatteryInfo> first_connected_device_;
  bluetooth_config::mojom::BluetoothModificationState modification_state_ =
      bluetooth_config::mojom::BluetoothModificationState::
          kCannotModifyBluetooth;
  bluetooth_config::mojom::BluetoothSystemState system_state_;
  raw_ptr<FeatureTile, DanglingUntriaged> tile_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<UnifiedSystemTrayController> tray_controller_;

  base::WeakPtrFactory<BluetoothFeaturePodController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_FEATURE_POD_CONTROLLER_H_

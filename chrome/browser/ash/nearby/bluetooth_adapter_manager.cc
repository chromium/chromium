// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/bluetooth_adapter_manager.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_client_uuids.h"
#include "device/bluetooth/adapter.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated. This enum should be kept in sync with the
// NearbyBluetoothAdapterAdvertisingSupport enum in
// //tools/metrics/histograms/metadata/nearby/enums.xml.
enum class NearbyBluetoothAdapterAdvertisingSupport {
  kExtendedAdvertisingAndScatternetDualRole = 0,
  kExtendedAdvertisingOnly = 1,
  kScatternetDualRoleOnly = 2,
  kLegacyAdvertisingOnly = 3,
  kMaxValue = kLegacyAdvertisingOnly,
};

void LogBluetoothAdapterAdvertisingSupport(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  bool is_extended_advertisement_supported =
      bluetooth_adapter->IsExtendedAdvertisementsAvailable();
  bool is_scatternet_dual_role_supported = base::Contains(
      bluetooth_adapter->GetSupportedRoles(),
      device::BluetoothAdapter::BluetoothRole::kCentralPeripheral);

  if (is_extended_advertisement_supported &&
      is_scatternet_dual_role_supported) {
    base::UmaHistogramEnumeration(
        "Nearby.BluetoothAdapter.AdvertisingSupport",
        NearbyBluetoothAdapterAdvertisingSupport::
            kExtendedAdvertisingAndScatternetDualRole);
    return;
  }

  if (is_extended_advertisement_supported) {
    base::UmaHistogramEnumeration(
        "Nearby.BluetoothAdapter.AdvertisingSupport",
        NearbyBluetoothAdapterAdvertisingSupport::kExtendedAdvertisingOnly);
    return;
  }

  if (is_scatternet_dual_role_supported) {
    base::UmaHistogramEnumeration(
        "Nearby.BluetoothAdapter.AdvertisingSupport",
        NearbyBluetoothAdapterAdvertisingSupport::kScatternetDualRoleOnly);
    return;
  }

  base::UmaHistogramEnumeration(
      "Nearby.BluetoothAdapter.AdvertisingSupport",
      NearbyBluetoothAdapterAdvertisingSupport::kLegacyAdvertisingOnly);
}

}  // namespace

namespace ash {
namespace nearby {

BluetoothAdapterManager::BluetoothAdapterManager() = default;
BluetoothAdapterManager::~BluetoothAdapterManager() = default;

void BluetoothAdapterManager::Initialize(
    mojo::PendingReceiver<bluetooth::mojom::Adapter> pending_receiver,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  LogBluetoothAdapterAdvertisingSupport(bluetooth_adapter);

  // It's safe to hold the raw pointer for |bluetooth_adapter| in
  // |device_bluetooth_adapter_| because ownership is passed to
  // |bluetooth_adapter_| which will stay around until this class is destroyed.
  device_bluetooth_adapter_ = bluetooth_adapter.get();
  bluetooth_adapter_ =
      std::make_unique<bluetooth::Adapter>(std::move(bluetooth_adapter));

  // Nearby Connections clients will be initiating and listening for connections
  // from the untrusted Nearby utility process. Pre-emptively allowlist their
  // UUIDs here in the browser process.
  for (const auto& uuid : GetNearbyClientUuids()) {
    bluetooth_adapter_->AllowConnectionsForUuid(uuid);
  }
  bluetooth_receiver_ =
      std::make_unique<mojo::Receiver<bluetooth::mojom::Adapter>>(
          bluetooth_adapter_.get(), std::move(pending_receiver));
}

void BluetoothAdapterManager::Shutdown() {
  if (!bluetooth_adapter_) {
    return;
  }

  // Close the mojo pipe which interrupts and unblocks all pending calls on the
  // other side.
  bluetooth_receiver_->reset();

  // Leave high-viz mode.  It's safe to always run this, regardless of the
  // actual high-visibility state, because Nearby is the only consumer of APIs
  // which change the device name and discoverable status.  However, if other
  // features use this in the future, this approach may need to be reconsidered.
  device_bluetooth_adapter_->SetDiscoverable(
      false, /*callback=*/base::DoNothing(),
      /*error_callback=*/base::DoNothing());
  device_bluetooth_adapter_->SetStandardChromeOSAdapterName();
  device_bluetooth_adapter_ = nullptr;
  bluetooth_adapter_.reset();
}

}  // namespace nearby
}  // namespace ash

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BLUETOOTH_BLUETOOTH_CHOOSER_CONTEXT_H_
#define CHROME_BROWSER_BLUETOOTH_BLUETOOTH_CHOOSER_CONTEXT_H_

#include <map>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "components/permissions/chooser_context_base.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-forward.h"

class Profile;

namespace base {
class Value;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

// Manages the permissions for Web Bluetooth device objects. A Web Bluetooth
// permission object consists of its WebBluetoothDeviceId and set of Bluetooth
// service UUIDs. The WebBluetoothDeviceId is generated randomly by this class
// and is unique for a given Bluetooth device address and origin pair, so this
// class stores this mapping and provides utility methods to convert between
// the WebBluetoothDeviceId and Bluetooth device address.
class BluetoothChooserContext : public permissions::ChooserContextBase {
 public:
  explicit BluetoothChooserContext(Profile* profile);
  ~BluetoothChooserContext() override;

  // Set class as move-only.
  BluetoothChooserContext(const BluetoothChooserContext&) = delete;
  BluetoothChooserContext& operator=(const BluetoothChooserContext&) = delete;

  // Helper methods for converting between a WebBluetoothDeviceId and a
  // Bluetooth device address string for a given origin pair.
  blink::WebBluetoothDeviceId GetWebBluetoothDeviceId(
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin,
      const std::string& device_address);
  std::string GetDeviceAddress(const url::Origin& requesting_origin,
                               const url::Origin& embedding_origin,
                               const blink::WebBluetoothDeviceId& device_id);

  // Bluetooth scanning specific interface for generating WebBluetoothDeviceIds
  // for scanned devices.
  blink::WebBluetoothDeviceId AddScannedDevice(
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin,
      const std::string& device_address);

  // Bluetooth-specific interface for granting and checking permissions.
  blink::WebBluetoothDeviceId GrantServiceAccessPermission(
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin,
      const device::BluetoothDevice* device,
      const blink::mojom::WebBluetoothRequestDeviceOptions* options);
  bool HasDevicePermission(const url::Origin& requesting_origin,
                           const url::Origin& embedding_origin,
                           const blink::WebBluetoothDeviceId& device_id);
  bool IsAllowedToAccessAtLeastOneService(
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin,
      const blink::WebBluetoothDeviceId& device_id);
  bool IsAllowedToAccessService(const url::Origin& requesting_origin,
                                const url::Origin& embedding_origin,
                                const blink::WebBluetoothDeviceId& device_id,
                                const device::BluetoothUUID& service);
  bool IsAllowedToAccessManufacturerData(
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin,
      const blink::WebBluetoothDeviceId& device_id,
      uint16_t manufacturer_code);

  static blink::WebBluetoothDeviceId GetObjectDeviceId(
      const base::Value& object);

  // ChooserContextBase;
  bool IsValidObject(const base::Value& object) override;
  base::string16 GetObjectDisplayName(const base::Value& object) override;

 private:
  base::Value FindDeviceObject(const url::Origin& requesting_origin,
                               const url::Origin& embedding_origin,
                               const blink::WebBluetoothDeviceId& device_id);

  // This map records the generated Web Bluetooth IDs for devices discovered via
  // the Scanning API. Each requesting/embedding origin pair has its own version
  // of this map so that IDs cannot be correlated between cross-origin sites.
  using DeviceAddressToIdMap =
      std::map<std::string, blink::WebBluetoothDeviceId>;
  std::map<std::pair<url::Origin, url::Origin>, DeviceAddressToIdMap>
      scanned_devices_;
};

#endif  // CHROME_BROWSER_BLUETOOTH_BLUETOOTH_CHOOSER_CONTEXT_H_

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_BLUETOOTH_BLUETOOTH_MOJOM_TRAITS_H_
#define ASH_COMPONENTS_ARC_BLUETOOTH_BLUETOOTH_MOJOM_TRAITS_H_

#include <memory>
#include <vector>

#include "ash/components/arc/mojom/bluetooth.mojom.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluez/bluetooth_service_attribute_value_bluez.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::BluetoothDeviceType, device::BluetoothTransport> {
  static arc::mojom::BluetoothDeviceType ToMojom(
      device::BluetoothTransport type) {
    switch (type) {
      case device::BLUETOOTH_TRANSPORT_CLASSIC:
        return arc::mojom::BluetoothDeviceType::BREDR;
      case device::BLUETOOTH_TRANSPORT_LE:
        return arc::mojom::BluetoothDeviceType::BLE;
      case device::BLUETOOTH_TRANSPORT_DUAL:
        return arc::mojom::BluetoothDeviceType::DUAL;
      default:
        DUMP_WILL_BE_NOTREACHED()
            << "Invalid type: " << static_cast<uint8_t>(type);
        // XXX: is there a better value to return here?
        return arc::mojom::BluetoothDeviceType::DUAL;
    }
  }

  static bool FromMojom(arc::mojom::BluetoothDeviceType mojom_type,
                        device::BluetoothTransport* type) {
    switch (mojom_type) {
      case arc::mojom::BluetoothDeviceType::BREDR:
        *type = device::BLUETOOTH_TRANSPORT_CLASSIC;
        break;
      case arc::mojom::BluetoothDeviceType::BLE:
        *type = device::BLUETOOTH_TRANSPORT_LE;
        break;
      case arc::mojom::BluetoothDeviceType::DUAL:
        *type = device::BLUETOOTH_TRANSPORT_DUAL;
        break;
      default:
        NOTREACHED() << "Invalid type: " << static_cast<uint32_t>(mojom_type);
    }
    return true;
  }
};

template <>
struct EnumTraits<arc::mojom::BluetoothSdpAttributeType,
                  bluez::BluetoothServiceAttributeValueBlueZ::Type> {
  static arc::mojom::BluetoothSdpAttributeType ToMojom(
      bluez::BluetoothServiceAttributeValueBlueZ::Type input) {
    switch (input) {
      case bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE:
      case bluez::BluetoothServiceAttributeValueBlueZ::UINT:
      case bluez::BluetoothServiceAttributeValueBlueZ::INT:
      case bluez::BluetoothServiceAttributeValueBlueZ::UUID:
      case bluez::BluetoothServiceAttributeValueBlueZ::STRING:
      case bluez::BluetoothServiceAttributeValueBlueZ::BOOL:
      case bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE:
      case bluez::BluetoothServiceAttributeValueBlueZ::URL:
        return static_cast<arc::mojom::BluetoothSdpAttributeType>(input);
      default:
        NOTREACHED() << "Invalid type: " << static_cast<uint32_t>(input);
    }
  }

  static bool FromMojom(
      arc::mojom::BluetoothSdpAttributeType input,
      bluez::BluetoothServiceAttributeValueBlueZ::Type* output) {
    switch (input) {
      case arc::mojom::BluetoothSdpAttributeType::NULLTYPE:
      case arc::mojom::BluetoothSdpAttributeType::UINT:
      case arc::mojom::BluetoothSdpAttributeType::INT:
      case arc::mojom::BluetoothSdpAttributeType::UUID:
      case arc::mojom::BluetoothSdpAttributeType::STRING:
      case arc::mojom::BluetoothSdpAttributeType::BOOL:
      case arc::mojom::BluetoothSdpAttributeType::SEQUENCE:
      case arc::mojom::BluetoothSdpAttributeType::URL:
        *output = static_cast<bluez::BluetoothServiceAttributeValueBlueZ::Type>(
            input);
        return true;
      default:
        NOTREACHED() << "Invalid type: " << static_cast<uint32_t>(input);
    }
  }
};

template <>
struct StructTraits<arc::mojom::BluetoothUUIDDataView, device::BluetoothUUID> {
  static std::vector<uint8_t> uuid(const device::BluetoothUUID& input);
  static bool Read(arc::mojom::BluetoothUUIDDataView data,
                   device::BluetoothUUID* output);
};

template <>
struct StructTraits<arc::mojom::BluetoothAdvertisementDataView,
                    std::unique_ptr<device::BluetoothAdvertisement::Data>> {
  static bool Read(
      arc::mojom::BluetoothAdvertisementDataView advertisement,
      std::unique_ptr<device::BluetoothAdvertisement::Data>* output);

  // Dummy methods.
  static arc::mojom::BluetoothAdvertisementType type(
      const std::unique_ptr<device::BluetoothAdvertisement::Data>& input) {
    NOTREACHED();
  }

  static bool include_tx_power(
      const std::unique_ptr<device::BluetoothAdvertisement::Data>& input) {
    NOTREACHED();
  }

  static std::vector<arc::mojom::BluetoothAdvertisingDataPtr> data(
      const std::unique_ptr<device::BluetoothAdvertisement::Data>& input) {
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // ASH_COMPONENTS_ARC_BLUETOOTH_BLUETOOTH_MOJOM_TRAITS_H_

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_BLUETOOTH_BLUETOOTH_TYPE_CONVERTERS_H_
#define ASH_COMPONENTS_ARC_BLUETOOTH_BLUETOOTH_TYPE_CONVERTERS_H_

#include <bluetooth/bluetooth.h>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>

#include "ash/components/arc/mojom/bluetooth.mojom.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_service_attribute_value_bluez.h"
#include "device/bluetooth/bluez/bluetooth_service_record_bluez.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace arc {
// The design of SDP attribute allows the attributes in the sequence of an
// attribute to be of sequence type. To prevent a malicious party from sending
// extremely deep attributes to cause the stack overflow, a maximum depth is
// enforced during the conversion between
// bluez::BluetoothServiceAttributeValueBlueZ and
// arc::mojom::BluetoothSdpAttributePtr. However, there is no assigned number
// defined in SDP specification, so we choose 32 as the limit based on the
// depths observed from various Bluetooth devices in the field.
constexpr size_t kBluetoothSDPMaxDepth = 32;
}  // namespace arc

namespace mojo {

template <>
struct TypeConverter<arc::mojom::BluetoothAddressPtr, std::string> {
  static arc::mojom::BluetoothAddressPtr Convert(const std::string& address);
};

template <>
struct TypeConverter<std::string, arc::mojom::BluetoothAddress> {
  static std::string Convert(const arc::mojom::BluetoothAddress& ptr);
};

template <>
struct TypeConverter<arc::mojom::BluetoothAddressPtr, bdaddr_t> {
  static arc::mojom::BluetoothAddressPtr Convert(const bdaddr_t& address);
};

template <>
struct TypeConverter<bdaddr_t, arc::mojom::BluetoothAddress> {
  static bdaddr_t Convert(const arc::mojom::BluetoothAddress& address);
};

template <>
struct TypeConverter<arc::mojom::BluetoothSdpAttributePtr,
                     bluez::BluetoothServiceAttributeValueBlueZ> {
  static arc::mojom::BluetoothSdpAttributePtr Convert(
      const bluez::BluetoothServiceAttributeValueBlueZ& attr_bluez,
      size_t depth);
  static arc::mojom::BluetoothSdpAttributePtr Convert(
      const bluez::BluetoothServiceAttributeValueBlueZ& attr_bluez) {
    return Convert(attr_bluez, 0);
  }
};

template <>
struct TypeConverter<bluez::BluetoothServiceAttributeValueBlueZ,
                     arc::mojom::BluetoothSdpAttributePtr> {
  static bluez::BluetoothServiceAttributeValueBlueZ Convert(
      const arc::mojom::BluetoothSdpAttributePtr& attr,
      size_t depth);
  static bluez::BluetoothServiceAttributeValueBlueZ Convert(
      const arc::mojom::BluetoothSdpAttributePtr& attr) {
    return Convert(attr, 0);
  }
};

template <>
struct TypeConverter<arc::mojom::BluetoothSdpRecordPtr,
                     bluez::BluetoothServiceRecordBlueZ> {
  static arc::mojom::BluetoothSdpRecordPtr Convert(
      const bluez::BluetoothServiceRecordBlueZ& rcd_bluez);
};

template <>
struct TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                     arc::mojom::BluetoothSdpRecordPtr> {
  static bluez::BluetoothServiceRecordBlueZ Convert(
      const arc::mojom::BluetoothSdpRecordPtr& rcd);
};

}  // namespace mojo

#endif  // ASH_COMPONENTS_ARC_BLUETOOTH_BLUETOOTH_TYPE_CONVERTERS_H_

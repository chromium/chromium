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
#include "device/bluetooth/floss/floss_sdp_types.h"
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

static const uint16_t ATTR_ID_SERVICE_NAME = 0x0100;
static const uint16_t ATTR_ID_SERVICE_CLASS_ID_LIST = 0x0001;
static const uint16_t ATTR_ID_SERVICE_ID = 0x0003;
static const uint16_t ATTR_ID_PROTOCOL_DESC_LIST = 0x0004;
static const uint16_t ATTR_ID_BT_PROFILE_DESC_LIST = 0x0009;
static const uint16_t ATTR_ID_SERVICE_DESCRIPTION = 0x0101;

/* Device Identification (DI)
 */
static const uint16_t ATTR_ID_SPECIFICATION_ID = 0x0200;
static const uint16_t ATTR_ID_VENDOR_ID = 0x0201;
static const uint16_t ATTR_ID_PRODUCT_ID = 0x0202;
static const uint16_t ATTR_ID_PRODUCT_VERSION = 0x0203;
static const uint16_t ATTR_ID_PRIMARY_RECORD = 0x0204;
static const uint16_t ATTR_ID_VENDOR_ID_SOURCE = 0x0205;

static const uint16_t ATTR_ID_SUPPORTED_FORMATS_LIST = 0x0303;
static const uint16_t ATTR_ID_SUPPORTED_FEATURES = 0x0311; /* HFP, BIP */
static const uint16_t ATTR_ID_SUPPORTED_REPOSITORIES =
    0x0314; /* Phone book access Profile */
static const uint16_t ATTR_ID_MAS_INSTANCE_ID = 0x0315;        /* MAP profile */
static const uint16_t ATTR_ID_SUPPORTED_MSG_TYPE = 0x0316;     /* MAP profile */
static const uint16_t ATTR_ID_MAP_SUPPORTED_FEATURES = 0x0317; /* MAP profile */
static const uint16_t ATTR_ID_PBAP_SUPPORTED_FEATURES =
    0x0317; /* PBAP profile */

static const uint16_t ATTR_ID_GOEP_L2CAP_PSM = 0x0200;

static const uint16_t UUID_PBAP_PCE = 0x112E;
static const uint16_t UUID_PBAP_PSE = 0x112F;
static const uint16_t UUID_MAP_MAS = 0x1132;
static const uint16_t UUID_SAP = 0x112D;
static const uint16_t UUID_SPP = 0x1101;
static const uint16_t UUID_DIP = 0x1200;
static const uint16_t UUID_MAP_MNS = 0x1133;

/* Define common 16-bit protocol UUIDs
 */
static const uint16_t UUID_PROTOCOL_RFCOMM = 0x0003;
static const uint16_t UUID_PROTOCOL_OBEX = 0x0008;
static const uint16_t UUID_PROTOCOL_BNEP = 0x000F;
static const uint16_t UUID_PROTOCOL_HIDP = 0x0011;
static const uint16_t UUID_PROTOCOL_AVCTP = 0x0017;
static const uint16_t UUID_PROTOCOL_AVDTP = 0x0019;
static const uint16_t UUID_PROTOCOL_L2CAP = 0x0100;
static const uint16_t UUID_PROTOCOL_ATT = 0x0007;

static const uint16_t UUID_SERVCLASS_MAP_PROFILE = 0x1134; /* MAP profile */
static const uint16_t UUID_SERVCLASS_PHONE_ACCESS = 0x1130;
static const uint16_t UUID_SERVCLASS_OBEX_OBJECT_PUSH = 0x1105;
static const uint16_t UUID_SERVCLASS_SAP = 0x112D; /* SIM Access profile */

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

template <>
struct TypeConverter<floss::BtSdpHeaderOverlay,
                     bluez::BluetoothServiceRecordBlueZ> {
  static floss::BtSdpHeaderOverlay Convert(
      const bluez::BluetoothServiceRecordBlueZ& bluez_record);
};

template <>
struct TypeConverter<floss::BtSdpMasRecord,
                     bluez::BluetoothServiceRecordBlueZ> {
  static floss::BtSdpMasRecord Convert(
      const bluez::BluetoothServiceRecordBlueZ& bluez_record);
};

template <>
struct TypeConverter<floss::BtSdpMnsRecord,
                     bluez::BluetoothServiceRecordBlueZ> {
  static floss::BtSdpMnsRecord Convert(
      const bluez::BluetoothServiceRecordBlueZ& bluez_record);
};

template <>
struct TypeConverter<floss::BtSdpPseRecord,
                     bluez::BluetoothServiceRecordBlueZ> {
  static floss::BtSdpPseRecord Convert(
      const bluez::BluetoothServiceRecordBlueZ& bluez_record);
};

template <>
struct TypeConverter<floss::BtSdpPceRecord,
                     bluez::BluetoothServiceRecordBlueZ> {
  static floss::BtSdpPceRecord Convert(
      const bluez::BluetoothServiceRecordBlueZ& bluez_record);
};

template <>
struct TypeConverter<floss::BtSdpOpsRecord,
                     bluez::BluetoothServiceRecordBlueZ> {
  static floss::BtSdpOpsRecord Convert(
      const bluez::BluetoothServiceRecordBlueZ& bluez_record);
};

template <>
struct TypeConverter<floss::BtSdpSapRecord,
                     bluez::BluetoothServiceRecordBlueZ> {
  static floss::BtSdpSapRecord Convert(
      const bluez::BluetoothServiceRecordBlueZ& bluez_record);
};

template <>
struct TypeConverter<floss::BtSdpDipRecord,
                     bluez::BluetoothServiceRecordBlueZ> {
  static floss::BtSdpDipRecord Convert(
      const bluez::BluetoothServiceRecordBlueZ& bluez_record);
};

template <>
struct TypeConverter<floss::BtSdpRecord, bluez::BluetoothServiceRecordBlueZ> {
  static floss::BtSdpRecord Convert(
      const bluez::BluetoothServiceRecordBlueZ& bluez_record);
};

template <>
struct TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                     floss::BtSdpHeaderOverlay> {
  static bluez::BluetoothServiceRecordBlueZ Convert(
      const floss::BtSdpHeaderOverlay& record);
};

template <>
struct TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                     floss::BtSdpMasRecord> {
  static bluez::BluetoothServiceRecordBlueZ Convert(
      const floss::BtSdpMasRecord& record);
};

template <>
struct TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                     floss::BtSdpMnsRecord> {
  static bluez::BluetoothServiceRecordBlueZ Convert(
      const floss::BtSdpMnsRecord& record);
};

template <>
struct TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                     floss::BtSdpPseRecord> {
  static bluez::BluetoothServiceRecordBlueZ Convert(
      const floss::BtSdpPseRecord& record);
};

template <>
struct TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                     floss::BtSdpPceRecord> {
  static bluez::BluetoothServiceRecordBlueZ Convert(
      const floss::BtSdpPceRecord& record);
};

template <>
struct TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                     floss::BtSdpOpsRecord> {
  static bluez::BluetoothServiceRecordBlueZ Convert(
      const floss::BtSdpOpsRecord& record);
};

template <>
struct TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                     floss::BtSdpSapRecord> {
  static bluez::BluetoothServiceRecordBlueZ Convert(
      const floss::BtSdpSapRecord& record);
};

template <>
struct TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                     floss::BtSdpDipRecord> {
  static bluez::BluetoothServiceRecordBlueZ Convert(
      const floss::BtSdpDipRecord& record);
};

template <>
struct TypeConverter<bluez::BluetoothServiceRecordBlueZ, floss::BtSdpRecord> {
  static bluez::BluetoothServiceRecordBlueZ Convert(
      const floss::BtSdpRecord& record);
};

}  // namespace mojo

#endif  // ASH_COMPONENTS_ARC_BLUETOOTH_BLUETOOTH_TYPE_CONVERTERS_H_

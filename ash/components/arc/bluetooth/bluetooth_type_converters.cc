// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iomanip>
#include <ios>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ash/components/arc/bluetooth/bluetooth_type_converters.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace {

constexpr size_t kAddressSize = 6;
constexpr char kInvalidAddress[] = "00:00:00:00:00:00";

// SDP Service attribute IDs.
constexpr uint16_t kServiceClassIDList = 0x0001;
constexpr uint16_t kProtocolDescriptorList = 0x0004;
constexpr uint16_t kBrowseGroupList = 0x0005;
constexpr uint16_t kBluetoothProfileDescriptorList = 0x0009;
constexpr uint16_t kServiceName = 0x0100;

}  // namespace

namespace mojo {

// static
arc::mojom::BluetoothAddressPtr
TypeConverter<arc::mojom::BluetoothAddressPtr, std::string>::Convert(
    const std::string& address) {
  arc::mojom::BluetoothAddressPtr mojo_addr =
      arc::mojom::BluetoothAddress::New();

  mojo_addr->address.resize(kAddressSize);
  if (!device::ParseBluetoothAddress(address, mojo_addr->address))
    mojo_addr->address.clear();

  return mojo_addr;
}

// static
std::string TypeConverter<std::string, arc::mojom::BluetoothAddress>::Convert(
    const arc::mojom::BluetoothAddress& address) {
  std::ostringstream addr_stream;
  addr_stream << std::setfill('0') << std::hex << std::uppercase;

  const std::vector<uint8_t>& bytes = address.address;

  if (address.address.size() != kAddressSize)
    return std::string(kInvalidAddress);

  for (size_t k = 0; k < bytes.size(); k++) {
    addr_stream << std::setw(2) << (unsigned int)bytes[k];
    addr_stream << ((k == bytes.size() - 1) ? "" : ":");
  }

  return addr_stream.str();
}

// static
arc::mojom::BluetoothAddressPtr
TypeConverter<arc::mojom::BluetoothAddressPtr, bdaddr_t>::Convert(
    const bdaddr_t& address) {
  arc::mojom::BluetoothAddressPtr mojo_addr =
      arc::mojom::BluetoothAddress::New();
  mojo_addr->address.resize(kAddressSize);
  std::reverse_copy(std::begin(address.b), std::end(address.b),
                    std::begin(mojo_addr->address));

  return mojo_addr;
}

// static
bdaddr_t TypeConverter<bdaddr_t, arc::mojom::BluetoothAddress>::Convert(
    const arc::mojom::BluetoothAddress& address) {
  bdaddr_t ret;
  std::reverse_copy(std::begin(address.address), std::end(address.address),
                    std::begin(ret.b));

  return ret;
}

// static
arc::mojom::BluetoothSdpAttributePtr
TypeConverter<arc::mojom::BluetoothSdpAttributePtr,
              bluez::BluetoothServiceAttributeValueBlueZ>::
    Convert(const bluez::BluetoothServiceAttributeValueBlueZ& attr_bluez,
            size_t depth) {
  auto result = arc::mojom::BluetoothSdpAttribute::New();
  result->type = attr_bluez.type();
  result->type_size = attr_bluez.size();
  switch (result->type) {
    case bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE:
      result->value = base::Value();
      return result;
    case bluez::BluetoothServiceAttributeValueBlueZ::UINT:
    case bluez::BluetoothServiceAttributeValueBlueZ::INT:
      result->value = base::Value(attr_bluez.value().GetInt());
      return result;
    case bluez::BluetoothServiceAttributeValueBlueZ::URL:
    case bluez::BluetoothServiceAttributeValueBlueZ::UUID:
    case bluez::BluetoothServiceAttributeValueBlueZ::STRING:
      result->value = base::Value(attr_bluez.value().GetString());
      return result;
    case bluez::BluetoothServiceAttributeValueBlueZ::BOOL:
      result->value = base::Value(attr_bluez.value().GetBool());
      return result;
    case bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE:
      if (depth + 1 >= arc::kBluetoothSDPMaxDepth)
        return Convert(bluez::BluetoothServiceAttributeValueBlueZ(), 0);
      for (const auto& child : attr_bluez.sequence())
        result->sequence.push_back(Convert(child, depth + 1));
      result->type_size = result->sequence.size();
      return result;
    default:
      NOTREACHED();
  }
}

// static
bluez::BluetoothServiceAttributeValueBlueZ
TypeConverter<bluez::BluetoothServiceAttributeValueBlueZ,
              arc::mojom::BluetoothSdpAttributePtr>::
    Convert(const arc::mojom::BluetoothSdpAttributePtr& attr, size_t depth) {
  bluez::BluetoothServiceAttributeValueBlueZ::Type type = attr->type;
  if (type != bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE &&
      !attr->value.has_value()) {
    return bluez::BluetoothServiceAttributeValueBlueZ();
  }

  switch (type) {
    case bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE:
      return bluez::BluetoothServiceAttributeValueBlueZ();
    case bluez::BluetoothServiceAttributeValueBlueZ::UINT:
    case bluez::BluetoothServiceAttributeValueBlueZ::INT:
    case bluez::BluetoothServiceAttributeValueBlueZ::URL:
    case bluez::BluetoothServiceAttributeValueBlueZ::UUID:
    case bluez::BluetoothServiceAttributeValueBlueZ::STRING:
    case bluez::BluetoothServiceAttributeValueBlueZ::BOOL:
      return bluez::BluetoothServiceAttributeValueBlueZ(type, attr->type_size,
                                                        attr->value->Clone());
    case bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE: {
      if (depth + 1 >= arc::kBluetoothSDPMaxDepth || attr->sequence.empty())
        return bluez::BluetoothServiceAttributeValueBlueZ();
      auto sequence = std::make_unique<
          bluez::BluetoothServiceAttributeValueBlueZ::Sequence>();
      for (const auto& child : attr->sequence)
        sequence->emplace_back(Convert(child, depth + 1));
      return bluez::BluetoothServiceAttributeValueBlueZ(std::move(sequence));
    }
    default:
      NOTREACHED();
  }
}

// static
arc::mojom::BluetoothSdpRecordPtr
TypeConverter<arc::mojom::BluetoothSdpRecordPtr,
              bluez::BluetoothServiceRecordBlueZ>::
    Convert(const bluez::BluetoothServiceRecordBlueZ& record_bluez) {
  arc::mojom::BluetoothSdpRecordPtr result =
      arc::mojom::BluetoothSdpRecord::New();

  for (auto id : record_bluez.GetAttributeIds()) {
    switch (id) {
      case kServiceClassIDList:
      case kProtocolDescriptorList:
      case kBrowseGroupList:
      case kBluetoothProfileDescriptorList:
      case kServiceName:
        result->attrs[id] = arc::mojom::BluetoothSdpAttribute::From(
            record_bluez.GetAttributeValue(id));
        break;
      default:
        // Android does not support this.
        break;
    }
  }

  return result;
}

// static
bluez::BluetoothServiceRecordBlueZ
TypeConverter<bluez::BluetoothServiceRecordBlueZ,
              arc::mojom::BluetoothSdpRecordPtr>::
    Convert(const arc::mojom::BluetoothSdpRecordPtr& record) {
  bluez::BluetoothServiceRecordBlueZ record_bluez;

  for (const auto& pair : record->attrs) {
    switch (pair.first) {
      case kServiceClassIDList:
      case kProtocolDescriptorList:
      case kBrowseGroupList:
      case kBluetoothProfileDescriptorList:
      case kServiceName:
        record_bluez.AddRecordEntry(
            pair.first,
            pair.second.To<bluez::BluetoothServiceAttributeValueBlueZ>());
        break;
      default:
        NOTREACHED();
    }
  }

  return record_bluez;
}

// Floss BtSdpRecord conversions adapted from
// aosp/packages/modules/Bluetooth/system/bta/sdp/bta_sdp_act.cc

// static
floss::BtSdpHeaderOverlay
TypeConverter<floss::BtSdpHeaderOverlay, bluez::BluetoothServiceRecordBlueZ>::
    Convert(const bluez::BluetoothServiceRecordBlueZ& bluez_record) {
  floss::BtSdpHeaderOverlay record_overlay{};
  // Set some default values that will be updated if bluez_record actually
  // contains relevant data.
  // The caller may change this type but for now assume this is generic record.
  record_overlay.sdp_type = floss::BtSdpType::kRaw;
  record_overlay.service_name_length = 0;
  record_overlay.service_name = "";
  record_overlay.rfcomm_channel_number = 0;
  record_overlay.l2cap_psm = -1;
  record_overlay.profile_version = 0;

  if (bluez_record.IsAttributePresented(ATTR_ID_SERVICE_NAME)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_SERVICE_NAME);
    const std::string* service_name = attribute.value().GetIfString();
    if (service_name) {
      record_overlay.service_name = *service_name;
      record_overlay.service_name_length = attribute.size();
    }
  }

  if (bluez_record.IsAttributePresented(ATTR_ID_SERVICE_CLASS_ID_LIST)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_SERVICE_CLASS_ID_LIST);
    for (const auto& serv_id : attribute.sequence()) {
      const std::string* uuid = serv_id.value().GetIfString();
      if (uuid) {
        record_overlay.uuid = device::BluetoothUUID(*uuid);
        break;
      }
    }
  }

  if (bluez_record.IsAttributePresented(ATTR_ID_PROTOCOL_DESC_LIST)) {
    const bluez::BluetoothServiceAttributeValueBlueZ maybe_protocol_list =
        bluez_record.GetAttributeValue(ATTR_ID_PROTOCOL_DESC_LIST);
    if (maybe_protocol_list.type() ==
        bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE) {
      auto protocol_list = maybe_protocol_list.sequence();
      for (auto protocol_record : protocol_list) {
        if (protocol_record.type() !=
            bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE) {
          continue;
        }
        auto protocol_record_sequence = protocol_record.sequence();
        // We expect at least two values: the UUID itself and the channel number
        if (protocol_record_sequence.size() < 2) {
          continue;
        }
        const std::string* uuid =
            protocol_record_sequence[0].value().GetIfString();
        if (!uuid) {
          continue;
        }
        std::vector<uint8_t> uuid_as_bytes =
            device::BluetoothUUID(*uuid).GetBytes();
        if (uuid_as_bytes.empty()) {
          break;
        }
        if ((static_cast<uint16_t>(uuid_as_bytes[2] << 8) |
             static_cast<uint16_t>(uuid_as_bytes[3])) != UUID_PROTOCOL_RFCOMM) {
          continue;
        }
        std::optional<int> channel_number =
            protocol_record_sequence[1].value().GetIfInt();
        if (!channel_number) {
          continue;
        }
        record_overlay.rfcomm_channel_number = *channel_number;
      }
    }
  }

  if (bluez_record.IsAttributePresented(ATTR_ID_GOEP_L2CAP_PSM)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_GOEP_L2CAP_PSM);
    std::optional<int> l2cap_psm = attribute.value().GetIfInt();
    if (l2cap_psm) {
      record_overlay.l2cap_psm = l2cap_psm.value();
    }
  }

  return record_overlay;
}

namespace {

// Following Core Specification V 5.3 | Vol 3, Part B
// Section 5.1.11 BluetoothProfileDescriptorList attribute
std::optional<int> GetProfileVersionFromBlueZRecord(
    const bluez::BluetoothServiceRecordBlueZ& bluez_record,
    const uint16_t profile_uuid) {
  if (!bluez_record.IsAttributePresented(ATTR_ID_BT_PROFILE_DESC_LIST)) {
    return std::nullopt;
  }
  if (!bluez_record.GetAttributeValue(ATTR_ID_BT_PROFILE_DESC_LIST)
           .is_sequence()) {
    return std::nullopt;
  }
  const auto profile_list =
      bluez_record.GetAttributeValue(ATTR_ID_BT_PROFILE_DESC_LIST).sequence();
  for (const auto& profile : profile_list) {
    if (!profile.is_sequence()) {
      continue;
    }
    const auto profile_descriptor = profile.sequence();
    if (profile_descriptor.size() < 2) {
      continue;
    }
    if (profile_descriptor[0].type() !=
        bluez::BluetoothServiceAttributeValueBlueZ::UUID) {
      continue;
    }
    if (profile_descriptor[0].value().GetInt() != profile_uuid) {
      continue;
    }
    if (profile_descriptor[1].type() !=
        bluez::BluetoothServiceAttributeValueBlueZ::UINT) {
      continue;
    }
    return profile_descriptor[1].value().GetIfInt();
  }
  return std::nullopt;
}

bluez::BluetoothServiceAttributeValueBlueZ MakeDescListForBlueZRecord(
    const uint16_t profile_or_protocol_uuid,
    const int version) {
  std::string maybe_short_uuid = base::NumberToString(profile_or_protocol_uuid);
  // L2CAP and RFCOMM ports/channels will not exceed 0x7FFF, but must be 4 hex
  // digits long for BluetoothUUID class to accept them.
  maybe_short_uuid.insert(maybe_short_uuid.begin(),
                          4 - maybe_short_uuid.length(), '0');
  const std::string full_uuid =
      device::BluetoothUUID(maybe_short_uuid).canonical_value();
  auto desc_list =
      std::make_unique<bluez::BluetoothServiceAttributeValueBlueZ::Sequence>();
  auto sequence =
      std::make_unique<bluez::BluetoothServiceAttributeValueBlueZ::Sequence>();
  sequence->emplace_back(bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::UUID, full_uuid.size(),
      std::optional<base::Value>(full_uuid)));
  sequence->emplace_back(bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::UINT, sizeof(version),
      std::optional<base::Value>(version)));
  desc_list->emplace_back(
      bluez::BluetoothServiceAttributeValueBlueZ(std::move(sequence)));
  return bluez::BluetoothServiceAttributeValueBlueZ(std::move(desc_list));
}

}  // namespace

// static
floss::BtSdpMasRecord
TypeConverter<floss::BtSdpMasRecord, bluez::BluetoothServiceRecordBlueZ>::
    Convert(const bluez::BluetoothServiceRecordBlueZ& bluez_record) {
  floss::BtSdpMasRecord mas_record{};
  mas_record.hdr =
      TypeConverter<floss::BtSdpHeaderOverlay,
                    bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
  mas_record.hdr.sdp_type = floss::BtSdpType::kMapMas;
  mas_record.mas_instance_id = 0;
  mas_record.supported_features = 0x0000001F;
  mas_record.supported_message_types = 0;

  if (bluez_record.IsAttributePresented(ATTR_ID_MAS_INSTANCE_ID)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_MAS_INSTANCE_ID);
    std::optional<int> mas_instance_id = attribute.value().GetIfInt();
    if (mas_instance_id) {
      mas_record.mas_instance_id = mas_instance_id.value();
    }
  }

  if (bluez_record.IsAttributePresented(ATTR_ID_SUPPORTED_MSG_TYPE)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_SUPPORTED_MSG_TYPE);
    std::optional<int> supported_message_types = attribute.value().GetIfInt();
    if (supported_message_types) {
      mas_record.supported_message_types = supported_message_types.value();
    }
  }

  if (bluez_record.IsAttributePresented(ATTR_ID_MAP_SUPPORTED_FEATURES)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_MAP_SUPPORTED_FEATURES);
    std::optional<int> supported_features = attribute.value().GetIfInt();
    if (supported_features) {
      mas_record.supported_features = supported_features.value();
    }
  }

  const std::optional<int> profile_version = GetProfileVersionFromBlueZRecord(
      bluez_record, UUID_SERVCLASS_MAP_PROFILE);
  if (profile_version.has_value()) {
    mas_record.hdr.profile_version = *profile_version;
  }

  return mas_record;
}

// static
floss::BtSdpMnsRecord
TypeConverter<floss::BtSdpMnsRecord, bluez::BluetoothServiceRecordBlueZ>::
    Convert(const bluez::BluetoothServiceRecordBlueZ& bluez_record) {
  floss::BtSdpMnsRecord mns_record{};
  mns_record.hdr =
      TypeConverter<floss::BtSdpHeaderOverlay,
                    bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
  mns_record.hdr.sdp_type = floss::BtSdpType::kMapMns;
  mns_record.supported_features = 0x0000001F;

  if (bluez_record.IsAttributePresented(ATTR_ID_MAP_SUPPORTED_FEATURES)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_MAP_SUPPORTED_FEATURES);
    std::optional<int> supported_features = attribute.value().GetIfInt();
    if (supported_features) {
      mns_record.supported_features = supported_features.value();
    }
  }

  const std::optional<int> profile_version = GetProfileVersionFromBlueZRecord(
      bluez_record, UUID_SERVCLASS_MAP_PROFILE);
  if (profile_version.has_value()) {
    mns_record.hdr.profile_version = *profile_version;
  }

  return mns_record;
}

// static
floss::BtSdpPseRecord
TypeConverter<floss::BtSdpPseRecord, bluez::BluetoothServiceRecordBlueZ>::
    Convert(const bluez::BluetoothServiceRecordBlueZ& bluez_record) {
  floss::BtSdpPseRecord pse_record{};
  pse_record.hdr =
      TypeConverter<floss::BtSdpHeaderOverlay,
                    bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
  ;
  pse_record.hdr.sdp_type = floss::BtSdpType::kPbapPse;
  pse_record.supported_features = 0x00000003;
  pse_record.supported_repositories = 0;

  if (bluez_record.IsAttributePresented(ATTR_ID_SUPPORTED_REPOSITORIES)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_SUPPORTED_REPOSITORIES);
    std::optional<int> supported_repositories = attribute.value().GetIfInt();
    if (supported_repositories) {
      pse_record.supported_repositories = supported_repositories.value();
    }
  }

  if (bluez_record.IsAttributePresented(ATTR_ID_SUPPORTED_FEATURES)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_SUPPORTED_FEATURES);
    std::optional<int> supported_features = attribute.value().GetIfInt();
    if (supported_features) {
      pse_record.supported_features = supported_features.value();
    }
  }

  const std::optional<int> profile_version = GetProfileVersionFromBlueZRecord(
      bluez_record, UUID_SERVCLASS_PHONE_ACCESS);
  if (profile_version.has_value()) {
    pse_record.hdr.profile_version = *profile_version;
  }

  return pse_record;
}

// static
floss::BtSdpPceRecord
TypeConverter<floss::BtSdpPceRecord, bluez::BluetoothServiceRecordBlueZ>::
    Convert(const bluez::BluetoothServiceRecordBlueZ& bluez_record) {
  floss::BtSdpPceRecord pce_record{};
  pce_record.hdr =
      TypeConverter<floss::BtSdpHeaderOverlay,
                    bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
  pce_record.hdr.sdp_type = floss::BtSdpType::kPbapPce;

  const std::optional<int> profile_version = GetProfileVersionFromBlueZRecord(
      bluez_record, UUID_SERVCLASS_PHONE_ACCESS);
  if (profile_version.has_value()) {
    pce_record.hdr.profile_version = *profile_version;
  }

  return pce_record;
}

// static
floss::BtSdpOpsRecord
TypeConverter<floss::BtSdpOpsRecord, bluez::BluetoothServiceRecordBlueZ>::
    Convert(const bluez::BluetoothServiceRecordBlueZ& bluez_record) {
  floss::BtSdpOpsRecord ops_record{};
  ops_record.hdr =
      TypeConverter<floss::BtSdpHeaderOverlay,
                    bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
  ops_record.hdr.sdp_type = floss::BtSdpType::kOppServer;

  const std::optional<int> profile_version = GetProfileVersionFromBlueZRecord(
      bluez_record, UUID_SERVCLASS_OBEX_OBJECT_PUSH);
  if (profile_version.has_value()) {
    ops_record.hdr.profile_version = *profile_version;
  }

  // TODO(b/277105543): Determine the correct structure for
  // supported_formats_list and implement conversion.

  return ops_record;
}

// static
floss::BtSdpSapRecord
TypeConverter<floss::BtSdpSapRecord, bluez::BluetoothServiceRecordBlueZ>::
    Convert(const bluez::BluetoothServiceRecordBlueZ& bluez_record) {
  floss::BtSdpSapRecord sap_record{};
  sap_record.hdr =
      TypeConverter<floss::BtSdpHeaderOverlay,
                    bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
  sap_record.hdr.sdp_type = floss::BtSdpType::kSapServer;

  const std::optional<int> profile_version =
      GetProfileVersionFromBlueZRecord(bluez_record, UUID_SERVCLASS_SAP);
  if (profile_version.has_value()) {
    sap_record.hdr.profile_version = *profile_version;
  }

  return sap_record;
}

// static
floss::BtSdpDipRecord
TypeConverter<floss::BtSdpDipRecord, bluez::BluetoothServiceRecordBlueZ>::
    Convert(const bluez::BluetoothServiceRecordBlueZ& bluez_record) {
  floss::BtSdpDipRecord dip_record{};
  dip_record.hdr =
      TypeConverter<floss::BtSdpHeaderOverlay,
                    bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
  dip_record.hdr.sdp_type = floss::BtSdpType::kDip;
  dip_record.spec_id = 0;
  dip_record.vendor = 0;
  dip_record.vendor_id_source = 0;
  dip_record.product = 0;
  dip_record.version = 0;
  dip_record.primary_record = false;

  if (bluez_record.IsAttributePresented(ATTR_ID_SPECIFICATION_ID)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_SPECIFICATION_ID);
    std::optional<int> spec_id = attribute.value().GetIfInt();
    if (spec_id) {
      dip_record.spec_id = spec_id.value();
    }
  }

  if (bluez_record.IsAttributePresented(ATTR_ID_VENDOR_ID)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_VENDOR_ID);
    std::optional<int> vendor = attribute.value().GetIfInt();
    if (vendor) {
      dip_record.vendor = vendor.value();
    }
  }

  if (bluez_record.IsAttributePresented(ATTR_ID_VENDOR_ID_SOURCE)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_VENDOR_ID_SOURCE);
    std::optional<int> vendor_id_source = attribute.value().GetIfInt();
    if (vendor_id_source) {
      dip_record.vendor_id_source = vendor_id_source.value();
    }
  }

  if (bluez_record.IsAttributePresented(ATTR_ID_PRODUCT_ID)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_PRODUCT_ID);
    std::optional<int> product = attribute.value().GetIfInt();
    if (product) {
      dip_record.product = product.value();
    }
  }

  if (bluez_record.IsAttributePresented(ATTR_ID_PRODUCT_VERSION)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_PRODUCT_VERSION);
    std::optional<int> version = attribute.value().GetIfInt();
    if (version) {
      dip_record.version = version.value();
    }
  }

  if (bluez_record.IsAttributePresented(ATTR_ID_PRIMARY_RECORD)) {
    const bluez::BluetoothServiceAttributeValueBlueZ attribute =
        bluez_record.GetAttributeValue(ATTR_ID_PRIMARY_RECORD);
    std::optional<bool> primary_record = attribute.value().GetIfBool();
    if (primary_record) {
      dip_record.primary_record = primary_record.value();
    }
  }

  return dip_record;
}

// static
floss::BtSdpRecord
TypeConverter<floss::BtSdpRecord, bluez::BluetoothServiceRecordBlueZ>::Convert(
    const bluez::BluetoothServiceRecordBlueZ& bluez_record) {
  if (!bluez_record.IsAttributePresented(ATTR_ID_SERVICE_ID)) {
    return TypeConverter<
        floss::BtSdpHeaderOverlay,
        bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
  }
  std::optional<int> service_id =
      bluez_record.GetAttributeValue(ATTR_ID_SERVICE_ID).value().GetIfInt();
  if (!service_id.has_value()) {
    return floss::BtSdpRecord();
  }
  switch (*service_id) {
    case UUID_MAP_MAS:
      return TypeConverter<
          floss::BtSdpMasRecord,
          bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
    case UUID_MAP_MNS:
      return TypeConverter<
          floss::BtSdpMnsRecord,
          bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
    case UUID_PBAP_PSE:
      return TypeConverter<
          floss::BtSdpPseRecord,
          bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
    case UUID_PBAP_PCE:
      return TypeConverter<
          floss::BtSdpPceRecord,
          bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
    case UUID_SPP:
      return TypeConverter<
          floss::BtSdpOpsRecord,
          bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
    case UUID_SAP:
      return TypeConverter<
          floss::BtSdpSapRecord,
          bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
    case UUID_DIP:
      return TypeConverter<
          floss::BtSdpDipRecord,
          bluez::BluetoothServiceRecordBlueZ>::Convert(bluez_record);
    default:
      return floss::BtSdpRecord();
  }
}

// static
bluez::BluetoothServiceRecordBlueZ
TypeConverter<bluez::BluetoothServiceRecordBlueZ, floss::BtSdpHeaderOverlay>::
    Convert(const floss::BtSdpHeaderOverlay& record) {
  bluez::BluetoothServiceRecordBlueZ bluez_record;

  bluez_record.AddRecordEntry(
      ATTR_ID_SERVICE_NAME,
      bluez::BluetoothServiceAttributeValueBlueZ(
          bluez::BluetoothServiceAttributeValueBlueZ::STRING,
          record.service_name_length,
          std::optional<base::Value>(record.service_name)));

  auto seq =
      std::make_unique<bluez::BluetoothServiceAttributeValueBlueZ::Sequence>();
  seq->emplace_back(bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::UUID,
      record.uuid.canonical_value().length(),
      std::optional<base::Value>(record.uuid.canonical_value())));
  bluez_record.AddRecordEntry(
      ATTR_ID_SERVICE_CLASS_ID_LIST,
      bluez::BluetoothServiceAttributeValueBlueZ(std::move(seq)));

  if (record.rfcomm_channel_number > 0) {
    bluez_record.AddRecordEntry(
        ATTR_ID_PROTOCOL_DESC_LIST,
        bluez::BluetoothServiceAttributeValueBlueZ(
            std::move(MakeDescListForBlueZRecord(
                UUID_PROTOCOL_RFCOMM, record.rfcomm_channel_number))));
  }

  if (record.l2cap_psm > -1) {
    bluez_record.AddRecordEntry(
        ATTR_ID_GOEP_L2CAP_PSM,
        bluez::BluetoothServiceAttributeValueBlueZ(
            bluez::BluetoothServiceAttributeValueBlueZ::UINT,
            sizeof(record.l2cap_psm),
            std::optional<base::Value>(record.l2cap_psm)));

    bluez_record.AddRecordEntry(
        ATTR_ID_PROTOCOL_DESC_LIST,
        bluez::BluetoothServiceAttributeValueBlueZ(
            std::move(MakeDescListForBlueZRecord(
                UUID_PROTOCOL_L2CAP, record.rfcomm_channel_number))));
  }

  return bluez_record;
}

// static
bluez::BluetoothServiceRecordBlueZ TypeConverter<
    bluez::BluetoothServiceRecordBlueZ,
    floss::BtSdpMasRecord>::Convert(const floss::BtSdpMasRecord& record) {
  bluez::BluetoothServiceRecordBlueZ bluez_record =
      TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                    floss::BtSdpHeaderOverlay>::Convert(record.hdr);

  bluez_record.AddRecordEntry(
      ATTR_ID_BT_PROFILE_DESC_LIST,
      bluez::BluetoothServiceAttributeValueBlueZ(
          std::move(MakeDescListForBlueZRecord(UUID_SERVCLASS_MAP_PROFILE,
                                               record.hdr.profile_version))));

  bluez_record.AddRecordEntry(
      ATTR_ID_MAS_INSTANCE_ID,
      bluez::BluetoothServiceAttributeValueBlueZ(
          bluez::BluetoothServiceAttributeValueBlueZ::UINT,
          sizeof(record.mas_instance_id),
          std::optional<base::Value>(
              static_cast<int>(record.mas_instance_id))));

  bluez_record.AddRecordEntry(
      ATTR_ID_SUPPORTED_MSG_TYPE,
      bluez::BluetoothServiceAttributeValueBlueZ(
          bluez::BluetoothServiceAttributeValueBlueZ::UINT,
          sizeof(record.supported_message_types),
          std::optional<base::Value>(
              static_cast<int>(record.supported_message_types))));

  bluez_record.AddRecordEntry(
      ATTR_ID_SUPPORTED_FEATURES,
      bluez::BluetoothServiceAttributeValueBlueZ(
          bluez::BluetoothServiceAttributeValueBlueZ::UINT,
          sizeof(record.supported_features),
          std::optional<base::Value>(
              static_cast<int>(record.supported_features))));

  return bluez_record;
}

// static
bluez::BluetoothServiceRecordBlueZ TypeConverter<
    bluez::BluetoothServiceRecordBlueZ,
    floss::BtSdpMnsRecord>::Convert(const floss::BtSdpMnsRecord& record) {
  bluez::BluetoothServiceRecordBlueZ bluez_record =
      TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                    floss::BtSdpHeaderOverlay>::Convert(record.hdr);

  bluez_record.AddRecordEntry(
      ATTR_ID_BT_PROFILE_DESC_LIST,
      bluez::BluetoothServiceAttributeValueBlueZ(
          std::move(MakeDescListForBlueZRecord(UUID_SERVCLASS_MAP_PROFILE,
                                               record.hdr.profile_version))));

  bluez_record.AddRecordEntry(
      ATTR_ID_SUPPORTED_FEATURES,
      bluez::BluetoothServiceAttributeValueBlueZ(
          bluez::BluetoothServiceAttributeValueBlueZ::UINT,
          sizeof(record.supported_features),
          std::optional<base::Value>(
              static_cast<int>(record.supported_features))));

  return bluez_record;
}

// static
bluez::BluetoothServiceRecordBlueZ TypeConverter<
    bluez::BluetoothServiceRecordBlueZ,
    floss::BtSdpPseRecord>::Convert(const floss::BtSdpPseRecord& record) {
  bluez::BluetoothServiceRecordBlueZ bluez_record =
      TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                    floss::BtSdpHeaderOverlay>::Convert(record.hdr);

  bluez_record.AddRecordEntry(
      ATTR_ID_BT_PROFILE_DESC_LIST,
      bluez::BluetoothServiceAttributeValueBlueZ(
          std::move(MakeDescListForBlueZRecord(UUID_SERVCLASS_PHONE_ACCESS,
                                               record.hdr.profile_version))));

  bluez_record.AddRecordEntry(
      ATTR_ID_SUPPORTED_REPOSITORIES,
      bluez::BluetoothServiceAttributeValueBlueZ(
          bluez::BluetoothServiceAttributeValueBlueZ::UINT,
          sizeof(record.supported_repositories),
          std::optional<base::Value>(
              static_cast<int>(record.supported_repositories))));

  bluez_record.AddRecordEntry(
      ATTR_ID_SUPPORTED_FEATURES,
      bluez::BluetoothServiceAttributeValueBlueZ(
          bluez::BluetoothServiceAttributeValueBlueZ::UINT,
          sizeof(record.supported_features),
          std::optional<base::Value>(
              static_cast<int>(record.supported_features))));

  return bluez_record;
}

// static
bluez::BluetoothServiceRecordBlueZ TypeConverter<
    bluez::BluetoothServiceRecordBlueZ,
    floss::BtSdpPceRecord>::Convert(const floss::BtSdpPceRecord& record) {
  bluez::BluetoothServiceRecordBlueZ bluez_record =
      TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                    floss::BtSdpHeaderOverlay>::Convert(record.hdr);

  bluez_record.AddRecordEntry(
      ATTR_ID_BT_PROFILE_DESC_LIST,
      bluez::BluetoothServiceAttributeValueBlueZ(
          std::move(MakeDescListForBlueZRecord(UUID_SERVCLASS_PHONE_ACCESS,
                                               record.hdr.profile_version))));

  return bluez_record;
}

// static
bluez::BluetoothServiceRecordBlueZ TypeConverter<
    bluez::BluetoothServiceRecordBlueZ,
    floss::BtSdpOpsRecord>::Convert(const floss::BtSdpOpsRecord& record) {
  bluez::BluetoothServiceRecordBlueZ bluez_record =
      TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                    floss::BtSdpHeaderOverlay>::Convert(record.hdr);

  bluez_record.AddRecordEntry(
      ATTR_ID_BT_PROFILE_DESC_LIST,
      bluez::BluetoothServiceAttributeValueBlueZ(
          std::move(MakeDescListForBlueZRecord(UUID_SERVCLASS_OBEX_OBJECT_PUSH,
                                               record.hdr.profile_version))));

  // TODO: supported_formats

  return bluez_record;
}

// static
bluez::BluetoothServiceRecordBlueZ TypeConverter<
    bluez::BluetoothServiceRecordBlueZ,
    floss::BtSdpSapRecord>::Convert(const floss::BtSdpSapRecord& record) {
  bluez::BluetoothServiceRecordBlueZ bluez_record =
      TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                    floss::BtSdpHeaderOverlay>::Convert(record.hdr);

  bluez_record.AddRecordEntry(
      ATTR_ID_BT_PROFILE_DESC_LIST,
      bluez::BluetoothServiceAttributeValueBlueZ(
          std::move(MakeDescListForBlueZRecord(UUID_SERVCLASS_SAP,
                                               record.hdr.profile_version))));

  return bluez_record;
}

// static
bluez::BluetoothServiceRecordBlueZ TypeConverter<
    bluez::BluetoothServiceRecordBlueZ,
    floss::BtSdpDipRecord>::Convert(const floss::BtSdpDipRecord& record) {
  bluez::BluetoothServiceRecordBlueZ bluez_record =
      TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                    floss::BtSdpHeaderOverlay>::Convert(record.hdr);
  // The following static_cast<int>() calls are being invoked on uint16_t fields
  // which can safely convert to int.
  bluez_record.AddRecordEntry(
      ATTR_ID_SPECIFICATION_ID,
      bluez::BluetoothServiceAttributeValueBlueZ(
          bluez::BluetoothServiceAttributeValueBlueZ::UINT,
          sizeof(record.spec_id),
          std::optional<base::Value>(static_cast<int>(record.spec_id))));

  bluez_record.AddRecordEntry(
      ATTR_ID_VENDOR_ID,
      bluez::BluetoothServiceAttributeValueBlueZ(
          bluez::BluetoothServiceAttributeValueBlueZ::UINT,
          sizeof(record.vendor),
          std::optional<base::Value>(static_cast<int>(record.vendor))));

  bluez_record.AddRecordEntry(
      ATTR_ID_VENDOR_ID_SOURCE,
      bluez::BluetoothServiceAttributeValueBlueZ(
          bluez::BluetoothServiceAttributeValueBlueZ::UINT,
          sizeof(record.vendor_id_source),
          std::optional<base::Value>(
              static_cast<int>(record.vendor_id_source))));

  bluez_record.AddRecordEntry(
      ATTR_ID_PRODUCT_ID,
      bluez::BluetoothServiceAttributeValueBlueZ(
          bluez::BluetoothServiceAttributeValueBlueZ::UINT,
          sizeof(record.product),
          std::optional<base::Value>(static_cast<int>(record.product))));

  bluez_record.AddRecordEntry(
      ATTR_ID_PRODUCT_VERSION,
      bluez::BluetoothServiceAttributeValueBlueZ(
          bluez::BluetoothServiceAttributeValueBlueZ::UINT,
          sizeof(record.version),
          std::optional<base::Value>(static_cast<int>(record.version))));

  bluez_record.AddRecordEntry(
      ATTR_ID_PRIMARY_RECORD,
      bluez::BluetoothServiceAttributeValueBlueZ(
          bluez::BluetoothServiceAttributeValueBlueZ::BOOL,
          sizeof(record.primary_record),
          std::optional<base::Value>(record.primary_record)));

  return bluez_record;
}

// static
bluez::BluetoothServiceRecordBlueZ
TypeConverter<bluez::BluetoothServiceRecordBlueZ, floss::BtSdpRecord>::Convert(
    const floss::BtSdpRecord& record) {
  if (absl::holds_alternative<floss::BtSdpHeaderOverlay>(record)) {
    return TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                         floss::BtSdpHeaderOverlay>::
        Convert(absl::get<floss::BtSdpHeaderOverlay>(record));
  } else if (absl::holds_alternative<floss::BtSdpMasRecord>(record)) {
    return TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                         floss::BtSdpMasRecord>::
        Convert(absl::get<floss::BtSdpMasRecord>(record));
  } else if (absl::holds_alternative<floss::BtSdpMnsRecord>(record)) {
    return TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                         floss::BtSdpMnsRecord>::
        Convert(absl::get<floss::BtSdpMnsRecord>(record));
  } else if (absl::holds_alternative<floss::BtSdpPseRecord>(record)) {
    return TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                         floss::BtSdpPseRecord>::
        Convert(absl::get<floss::BtSdpPseRecord>(record));
  } else if (absl::holds_alternative<floss::BtSdpPceRecord>(record)) {
    return TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                         floss::BtSdpPceRecord>::
        Convert(absl::get<floss::BtSdpPceRecord>(record));
  } else if (absl::holds_alternative<floss::BtSdpOpsRecord>(record)) {
    return TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                         floss::BtSdpOpsRecord>::
        Convert(absl::get<floss::BtSdpOpsRecord>(record));
  } else if (absl::holds_alternative<floss::BtSdpSapRecord>(record)) {
    return TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                         floss::BtSdpSapRecord>::
        Convert(absl::get<floss::BtSdpSapRecord>(record));
  } else if (absl::holds_alternative<floss::BtSdpDipRecord>(record)) {
    return TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                         floss::BtSdpDipRecord>::
        Convert(absl::get<floss::BtSdpDipRecord>(record));
  } else {
    return bluez::BluetoothServiceRecordBlueZ();
  }
}

}  // namespace mojo

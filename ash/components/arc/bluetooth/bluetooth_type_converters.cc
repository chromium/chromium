// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cctype>
#include <iomanip>
#include <ios>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ash/components/arc/bluetooth/bluetooth_type_converters.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
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
      return bluez::BluetoothServiceAttributeValueBlueZ(
          type, attr->type_size,
          std::make_unique<base::Value>(attr->value->GetInt()));
    case bluez::BluetoothServiceAttributeValueBlueZ::URL:
    case bluez::BluetoothServiceAttributeValueBlueZ::UUID:
    case bluez::BluetoothServiceAttributeValueBlueZ::STRING:
      return bluez::BluetoothServiceAttributeValueBlueZ(
          type, attr->type_size,
          std::make_unique<base::Value>(attr->value->GetString()));
    case bluez::BluetoothServiceAttributeValueBlueZ::BOOL:
      return bluez::BluetoothServiceAttributeValueBlueZ(
          type, attr->type_size,
          std::make_unique<base::Value>(attr->value->GetBool()));
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
        break;
    }
  }

  return record_bluez;
}

}  // namespace mojo

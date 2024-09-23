// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_blocklist.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <tuple>

#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace {

const char kEntryKeyUsb[] = "usb";
const char kEntryKeyBluetooth[] = "bluetooth";
const char kBluetoothStandardUUID[] = "0000-1000-8000-00805f9b34fb";

// Returns true if the passed string is exactly 4 digits long and only contains
// valid hexadecimal characters (no leading 0x).
bool IsHexComponent(std::string_view string) {
  if (string.length() != 4)
    return false;

  // This is necessary because base::HexStringToUInt allows whitespace and the
  // "0x" prefix in its input.
  for (char c : string) {
    if (c >= '0' && c <= '9')
      continue;
    if (c >= 'a' && c <= 'f')
      continue;
    if (c >= 'A' && c <= 'F')
      continue;
    return false;
  }
  return true;
}

bool CompareEntry(const SerialBlocklist::Entry& a,
                  const SerialBlocklist::Entry& b) {
  return std::tie(a.usb_vendor_id, a.usb_product_id,
                  a.bluetooth_service_class_id) <
         std::tie(b.usb_vendor_id, b.usb_product_id,
                  b.bluetooth_service_class_id);
}

// Returns true if an entry in [begin, end) matches |entry|.
template <class Iterator>
bool EntryMatches(Iterator begin,
                  Iterator end,
                  const SerialBlocklist::Entry& entry) {
  auto it = std::lower_bound(begin, end, entry, CompareEntry);
  if (it == end) {
    return false;
  }
  // USB devices
  if (it->bluetooth_service_class_id.empty()) {
    return it->usb_vendor_id == entry.usb_vendor_id &&
           it->usb_product_id == entry.usb_product_id;
  }
  // Bluetooth devices
  return it->bluetooth_service_class_id == entry.bluetooth_service_class_id;
}

}  // namespace

BASE_FEATURE(kWebSerialBlocklist,
             "WebSerialBlocklist",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<std::string> kWebSerialBlocklistAdditions{
    &kWebSerialBlocklist, "BlocklistAdditions", /*default_value=*/""};

SerialBlocklist::~SerialBlocklist() = default;

// static
SerialBlocklist& SerialBlocklist::Get() {
  static base::NoDestructor<SerialBlocklist> blocklist;
  return *blocklist;
}

bool SerialBlocklist::IsExcluded(
    const device::mojom::SerialPortInfo& port_info) const {
  if (!((port_info.has_vendor_id && port_info.has_product_id) ||
        port_info.bluetooth_service_class_id.has_value())) {
    return false;
  }

  // We exclude all Bluetooth specified services except Serial Port Profile
  // regardless of the blocklist for security.
  if (port_info.bluetooth_service_class_id &&
      port_info.bluetooth_service_class_id->IsValid() &&
      *port_info.bluetooth_service_class_id !=
          device::GetSerialPortProfileUUID() &&
      port_info.bluetooth_service_class_id->canonical_value().substr(9, 27) ==
          kBluetoothStandardUUID) {
    return true;
  }

  Entry entry(port_info.has_vendor_id ? port_info.vendor_id : 0x0,
              port_info.has_product_id ? port_info.product_id : 0x0,
              port_info.bluetooth_service_class_id
                  ? port_info.bluetooth_service_class_id->canonical_value()
                  : "");
  return EntryMatches(std::begin(static_entries_), std::end(static_entries_),
                      entry) ||
         EntryMatches(dynamic_entries_.begin(), dynamic_entries_.end(), entry);
}

void SerialBlocklist::ResetToDefaultValuesForTesting() {
  dynamic_entries_.clear();
  PopulateWithServerProvidedValues();
}

SerialBlocklist::SerialBlocklist() {
  CHECK(std::is_sorted(std::begin(static_entries_), std::end(static_entries_),
                       CompareEntry));
  PopulateWithServerProvidedValues();
}

void SerialBlocklist::PopulateWithServerProvidedValues() {
  std::string blocklist_string = kWebSerialBlocklistAdditions.Get();

  for (const auto& entry :
       base::SplitStringPiece(blocklist_string, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string_view> components = base::SplitStringPiece(
        entry, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    // Entry must at least indicate the type
    if (components.empty()) {
      continue;
    }

    if (components[0] == kEntryKeyUsb) {
      if (components.size() != 3 || !IsHexComponent(components[1]) ||
          !IsHexComponent(components[2])) {
        continue;
      }

      uint32_t vendor_id;
      uint32_t product_id;
      if (!base::HexStringToUInt(components[1], &vendor_id) ||
          !base::HexStringToUInt(components[2], &product_id)) {
        continue;
      }

      dynamic_entries_.emplace_back(vendor_id, product_id,
                                    /*bluetooth_service_class_id=*/"");
    } else if (components[0] == kEntryKeyBluetooth) {
      if (components.size() != 2) {
        continue;
      }

      std::string uuid_as_string(components[1]);
      device::BluetoothUUID uuid(uuid_as_string);
      if (!uuid.IsValid()) {
        continue;
      }

      dynamic_entries_.emplace_back(/*usb_vendor_id=*/0, /*usb_product_id=*/0,
                                    uuid.canonical_value());
    }
  }

  std::sort(dynamic_entries_.begin(), dynamic_entries_.end(), CompareEntry);
}

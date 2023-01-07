// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_blocklist.h"

#include <algorithm>
#include <string>
#include <tuple>

#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace {

// Returns true if the passed string is exactly 4 digits long and only contains
// valid hexadecimal characters (no leading 0x).
bool IsHexComponent(base::StringPiece string) {
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
  return std::tie(a.usb_vendor_id, a.usb_product_id) <
         std::tie(b.usb_vendor_id, b.usb_product_id);
}

// Returns true if an entry in [begin, end) matches the vendor and product IDs
// of |entry| and has a device version greater than or equal to |entry|.
template <class Iterator>
bool EntryMatches(Iterator begin,
                  Iterator end,
                  const SerialBlocklist::Entry& entry) {
  auto it = std::lower_bound(begin, end, entry, CompareEntry);
  return it != end && it->usb_vendor_id == entry.usb_vendor_id &&
         it->usb_product_id == entry.usb_product_id;
}

// This list must be sorted according to CompareEntry.
constexpr SerialBlocklist::Entry kStaticEntries[] = {
    {0x18D1, 0x58F3},  // Test entry: GOOGLE_HID_ECHO_GADGET
};

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
  // Only USB devices can be matched.
  if (!port_info.has_vendor_id || !port_info.has_product_id) {
    return false;
  }

  Entry entry(port_info.vendor_id, port_info.product_id);
  return EntryMatches(std::begin(kStaticEntries), std::end(kStaticEntries),
                      entry) ||
         EntryMatches(dynamic_entries_.begin(), dynamic_entries_.end(), entry);
}

void SerialBlocklist::ResetToDefaultValuesForTesting() {
  dynamic_entries_.clear();
  PopulateWithServerProvidedValues();
}

SerialBlocklist::SerialBlocklist() {
  DCHECK(std::is_sorted(std::begin(kStaticEntries), std::end(kStaticEntries),
                        CompareEntry));
  PopulateWithServerProvidedValues();
}

void SerialBlocklist::PopulateWithServerProvidedValues() {
  std::string blocklist_string = kWebSerialBlocklistAdditions.Get();

  for (const auto& entry :
       base::SplitStringPiece(blocklist_string, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    std::vector<base::StringPiece> components = base::SplitStringPiece(
        entry, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (components.size() != 3 || components[0] != "usb" ||
        !IsHexComponent(components[1]) || !IsHexComponent(components[2])) {
      continue;
    }

    uint32_t vendor_id;
    uint32_t product_id;
    if (!base::HexStringToUInt(components[1], &vendor_id) ||
        !base::HexStringToUInt(components[2], &product_id)) {
      continue;
    }

    dynamic_entries_.emplace_back(vendor_id, product_id);
  }

  std::sort(dynamic_entries_.begin(), dynamic_entries_.end(), CompareEntry);
}

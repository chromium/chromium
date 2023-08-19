// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERIAL_SERIAL_BLOCKLIST_H_
#define CHROME_BROWSER_SERIAL_SERIAL_BLOCKLIST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "services/device/public/mojom/serial.mojom-forward.h"

// Feature used to configure entries in the Web Serial API blocklist which can
// be deployed using a server configuration.
BASE_DECLARE_FEATURE(kWebSerialBlocklist);

// Dynamic additions to the Web Serial API device blocklist.
//
// The string must be a comma-separated list of entries which start with a type
// identifier. Entries may be separated by an arbitrary amount of whitespace.
//
// USB - "usb:[vendor_id]:[product_id]
// A USB entry provides a vendor ID and product ID, each a 16-bit integer
// written as exactly 4 hexadecimal digits. For example, the entry
// "usb:1000:001C" matches a device with a vendor ID of 0x1000 and a product
// ID of 0x001C.
//
// Bluetooth - "bluetooth:[128-bit UUID as a string]" A Bluetooth entry provides
// the full 128-bit UUID of the service as a string.  The UUID is parsed by
// device::BluetoothUUID which requires the xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
// format as input for UUIDs not specified by the Bluetooth standard. Do not
// include Bluetooth specified UUIDs as they are already blocked (except for
// Serial Port Profile).
//
// Invalid entries in the list will be ignored.
extern const base::FeatureParam<std::string> kWebSerialBlocklistAdditions;

class SerialBlocklist final {
 public:
  // An entry in the blocklist. Represents a device that should not be
  // accessible using the Web Serial API.
  struct Entry {
    Entry(uint16_t usb_vendor_id,
          uint16_t usb_product_id,
          std::string bluetooth_service_class_id)
        : usb_vendor_id(usb_vendor_id),
          usb_product_id(usb_product_id),
          bluetooth_service_class_id(std::move(bluetooth_service_class_id)) {}

    // Matched against the idVendor field of the USB Device Descriptor.
    uint16_t usb_vendor_id;

    // Matched against the idProduct field of the USB Device Descriptor.
    uint16_t usb_product_id;

    // Matched against the service class ID of a Bluetooth serial port.
    std::string bluetooth_service_class_id;
  };

  SerialBlocklist(const SerialBlocklist&) = delete;
  SerialBlocklist& operator=(const SerialBlocklist&) = delete;
  ~SerialBlocklist();

  // Returns a singleton instance of the blocklist.
  static SerialBlocklist& Get();

  // Returns if a device is excluded from access.
  bool IsExcluded(const device::mojom::SerialPortInfo& port_info) const;

  // Size of the blocklist.
  size_t GetDynamicEntryCountForTesting() const {
    return dynamic_entries_.size();
  }

  // Reload the blocklist for testing purposes.
  void ResetToDefaultValuesForTesting();

 private:
  // Friend NoDestructor to permit access to private constructor.
  friend class base::NoDestructor<SerialBlocklist>;

  SerialBlocklist();

  // Populates the blocklist with values set via a Finch experiment which allows
  // the set of blocked devices to be updated without shipping new executable
  // versions.
  //
  // See kWebSerialBlocklistAdditions for the format of this parameter.
  void PopulateWithServerProvidedValues();

  // Set of static blocklist entries.
  std::vector<Entry> static_entries_{
      Entry(0x18D1, 0x58F3, "")  // Test entry: GOOGLE_HID_ECHO_GADGET
  };
  // Set of blocklist entries from the server.
  std::vector<Entry> dynamic_entries_;
};

#endif  // CHROME_BROWSER_SERIAL_SERIAL_BLOCKLIST_H_

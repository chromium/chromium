// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/usb_printer_util.h"

#include <ctype.h>
#include <stdint.h>
#include <vector>

#include "base/big_endian.h"
#include "base/md5.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/printing/printer_configuration.h"
#include "device/usb/public/cpp/usb_utils.h"
#include "device/usb/public/mojom/device_manager.mojom.h"
#include "device/usb/usb_device.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace {

// Base class used for printer USB interfaces
// (https://www.usb.org/developers/defined_class).
constexpr uint8_t kPrinterInterfaceClass = 7;

// Subclass used for printers
// (http://www.usb.org/developers/docs/devclass_docs/usbprint11a021811.pdf).
constexpr uint8_t kPrinterInterfaceSubclass = 1;

// Protocol for ippusb printing.
// (http://www.usb.org/developers/docs/devclass_docs/IPP.zip).
constexpr uint8_t kPrinterIppusbProtocol = 4;

// Escape URI strings the same way cups does it, so we end up with a URI cups
// recognizes.  Cups hex-encodes '%', ' ', and anything not in the standard
// ASCII range.  CUPS lets everything else through unchanged.
//
// TODO(justincarlson): Determine whether we should apply escaping at the
// outgoing edge, when we send Printer information to CUPS, instead of
// pre-escaping at the point the field is filled in.
//
// https://crbug.com/701606
std::string CupsURIEscape(const std::string& uri_in) {
  static const char kHexDigits[] = "0123456789ABCDEF";
  std::vector<char> buf;
  buf.reserve(uri_in.size());
  for (char c : uri_in) {
    if (c == ' ' || c == '%' || (c & 0x80)) {
      buf.push_back('%');
      buf.push_back(kHexDigits[(c >> 4) & 0xf]);
      buf.push_back(kHexDigits[c & 0xf]);
    } else {
      buf.push_back(c);
    }
  }
  return std::string(buf.data(), buf.size());
}

// Incorporate the bytes of |val| into the incremental hash carried in |ctx| in
// big-endian order.  |val| must be a simple integer type
template <typename T>
void MD5UpdateBigEndian(base::MD5Context* ctx, T val) {
  static_assert(std::is_integral<T>::value, "Value must be an integer");
  char buf[sizeof(T)];
  base::WriteBigEndian(buf, val);
  base::MD5Update(ctx, base::StringPiece(buf, sizeof(T)));
}

// Update the hash with the contents of |str|.
//
// UTF-16 strings are a bit fraught for consistency in memory representation;
// endianness is an issue, but more importantly, there are *optional* prefix
// codepoints to identify the endianness of the string.
//
// This is a long way to say "UTF-16 is hard to hash, let's just convert
// to UTF-8 and hash that", which avoids all of these issues.
void MD5UpdateString16(base::MD5Context* ctx, const base::string16& str) {
  std::string tmp = base::UTF16ToUTF8(str);
  base::MD5Update(ctx, base::StringPiece(tmp.data(), tmp.size()));
}

// Get the usb printer id for |device|.  This is used both as the identifier for
// the printer in the user's PrintersManager and as the name of the printer in
// CUPS, so it has to satisfy the naming restrictions of both.  CUPS in
// particular is intolerant of much more than [a-z0-9_-], so we use that
// character set.  This needs to be stable for a given device, but as unique as
// possible for that device.  So we basically toss every bit of stable
// information from the device into an MD5 hash, and then hexify the hash value
// as a suffix to "usb-" as the final printer id.
std::string UsbPrinterId(const device::UsbDevice& device) {
  // Paranoid checks; in the unlikely event someone messes with the USB device
  // definition, our (supposedly stable) hashes will change.
  static_assert(sizeof(device.device_class()) == 1, "Class size changed");
  static_assert(sizeof(device.device_subclass()) == 1, "Subclass size changed");
  static_assert(sizeof(device.device_protocol()) == 1, "Protocol size changed");
  static_assert(sizeof(device.vendor_id()) == 2, "Vendor id size changed");
  static_assert(sizeof(device.product_id()) == 2, "Product id size changed");
  static_assert(sizeof(device.device_version()) == 2, "Version size changed");

  base::MD5Context ctx;
  base::MD5Init(&ctx);
  MD5UpdateBigEndian(&ctx, device.device_class());
  MD5UpdateBigEndian(&ctx, device.device_subclass());
  MD5UpdateBigEndian(&ctx, device.device_protocol());
  MD5UpdateBigEndian(&ctx, device.vendor_id());
  MD5UpdateBigEndian(&ctx, device.product_id());
  MD5UpdateBigEndian(&ctx, device.device_version());
  MD5UpdateString16(&ctx, device.manufacturer_string());
  MD5UpdateString16(&ctx, device.product_string());
  MD5UpdateString16(&ctx, device.serial_number());
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  return base::StringPrintf("usb-%s", base::MD5DigestToBase16(digest).c_str());
}

}  // namespace

// Creates a mojom filter which can be used to identify a basic USB printer.
mojo::StructPtr<device::mojom::UsbDeviceFilter> CreatePrinterFilter() {
  auto printer_filter = device::mojom::UsbDeviceFilter::New();
  printer_filter->has_class_code = true;
  printer_filter->class_code = kPrinterInterfaceClass;
  printer_filter->has_subclass_code = true;
  printer_filter->subclass_code = kPrinterInterfaceSubclass;

  return printer_filter;
}

bool UsbDeviceIsPrinter(const device::UsbDevice& usb_device) {
  auto printer_filter = CreatePrinterFilter();
  return UsbDeviceFilterMatches(*printer_filter, usb_device);
}

bool UsbDeviceSupportsIppusb(const device::UsbDevice& usb_device) {
  auto printer_filter = CreatePrinterFilter();
  printer_filter->has_protocol_code = true;
  printer_filter->protocol_code = kPrinterIppusbProtocol;

  return UsbDeviceFilterMatches(*printer_filter, usb_device);
}

std::string UsbPrinterDeviceDetailsAsString(const device::UsbDevice& device) {
  return base::StringPrintf(
      " guid:                %s\n"
      " usb version:         %d\n"
      " device class:        %d\n"
      " device subclass:     %d\n"
      " device protocol:     %d\n"
      " vendor id:           %04x\n"
      " product id:          %04x\n"
      " device version:      %d\n"
      " manufacturer string: %s\n"
      " product string:      %s\n"
      " serial number:       %s",
      device.guid().c_str(), device.usb_version(), device.device_class(),
      device.device_subclass(), device.device_protocol(), device.vendor_id(),
      device.product_id(), device.device_version(),
      base::UTF16ToUTF8(device.manufacturer_string()).c_str(),
      base::UTF16ToUTF8(device.product_string()).c_str(),
      base::UTF16ToUTF8(device.serial_number()).c_str());
}

// Attempt to gather all the information we need to work with this printer by
// querying the USB device.  This should only be called using devices we believe
// are printers, not arbitrary USB devices, as we may get weird partial results
// from arbitrary devices.
std::unique_ptr<Printer> UsbDeviceToPrinter(const device::UsbDevice& device) {
  // Preflight all required fields and log errors if we find something wrong.
  if (device.vendor_id() == 0 || device.product_id() == 0) {
    LOG(ERROR) << "Failed to convert USB device to printer.  Fields were:\n"
               << UsbPrinterDeviceDetailsAsString(device);
    return nullptr;
  }

  auto printer = std::make_unique<Printer>();
  printer->set_manufacturer(base::UTF16ToUTF8(device.manufacturer_string()));
  printer->set_model(base::UTF16ToUTF8(device.product_string()));
  // Synthesize make-and-model string for printer identification.
  printer->set_make_and_model(
      base::JoinString({printer->manufacturer(), printer->model()}, " "));

  // Construct the display name by however much of the manufacturer/model
  // information that we have available.
  if (printer->manufacturer().empty() && printer->model().empty()) {
    printer->set_display_name(
        l10n_util::GetStringUTF8(IDS_USB_PRINTER_UNKNOWN_DISPLAY_NAME));
  } else if (!printer->manufacturer().empty() && !printer->model().empty()) {
    printer->set_display_name(
        l10n_util::GetStringFUTF8(IDS_USB_PRINTER_DISPLAY_NAME,
                                  base::UTF8ToUTF16(printer->manufacturer()),
                                  base::UTF8ToUTF16(printer->model())));
  } else {
    // Exactly one string is present.
    std::string non_empty = !printer->manufacturer().empty()
                                ? printer->manufacturer()
                                : printer->model();
    printer->set_display_name(
        l10n_util::GetStringFUTF8(IDS_USB_PRINTER_DISPLAY_NAME_MAKE_OR_MODEL,
                                  base::UTF8ToUTF16(non_empty)));
  }

  printer->set_description(printer->display_name());
  printer->set_uri(UsbPrinterUri(device));
  printer->set_id(UsbPrinterId(device));
  printer->set_supports_ippusb(UsbDeviceSupportsIppusb(device));
  return printer;
}

std::string UsbPrinterUri(const device::UsbDevice& device) {
  // Note that serial may, for some devices, be empty or bogus (all zeros, non
  // unique, or otherwise not really a serial number), but having a non-unique
  // or empty serial field in the URI still lets us print, it just means we
  // don't have a way to uniquely identify a printer if there are multiple ones
  // plugged in with the same VID/PID, so we may print to the *wrong* printer.
  // There doesn't seem to be a robust solution to this problem; if printers
  // don't supply a serial number, we don't have any reliable way to do that
  // differentiation.
  std::string serial = base::UTF16ToUTF8(device.serial_number());
  return CupsURIEscape(base::StringPrintf("usb://%04x/%04x?serial=%s",
                                          device.vendor_id(),
                                          device.product_id(), serial.c_str()));
}

}  // namespace chromeos

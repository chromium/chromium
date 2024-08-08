// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/usb_printer_util.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/md5.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/usb_printer_id.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

using device::mojom::UsbDeviceInfo;

// Base class used for printer USB interfaces
// (https://www.usb.org/developers/defined_class).
constexpr uint8_t kPrinterInterfaceClass = 7;

// Subclass used for printers
// (http://www.usb.org/developers/docs/devclass_docs/usbprint11a021811.pdf).
constexpr uint8_t kPrinterInterfaceSubclass = 1;

// Protocol for ippusb printing.
// (http://www.usb.org/developers/docs/devclass_docs/IPP.zip).
constexpr uint8_t kPrinterIppusbProtocol = 4;

// Configuration for a GET_DEVICE_ID Printer Class-Specific Request.
const int kGetDeviceIdRequest = 0;
const int kDefaultInterface = 0;
const int kDefaultConfiguration = 0;

// Generic USB make_and_model strings that are reused across device.
bool IsGenericUsbDescription(const std::string& make_and_model) {
  static constexpr auto kGenericUsbModels =
      base::MakeFixedFlatSet<std::string_view>({
          "canon canon capt usb device",
          "epson usb1.1 mfp(hi-speed)",
          "epson usb2.0 mfp(hi-speed)",
          "epson usb2.0 printer (hi-speed)",
          "epson usb mfp",
          "epson usb printer",
          "seiko epson usb mfp",
          "oki data corp usb device",
      });
  return base::Contains(kGenericUsbModels, make_and_model);
}

// Callback for device.mojom.UsbDevice.ControlTransferIn.
// Expects |data| to hold a newly queried Device ID.
void OnControlTransfer(mojo::Remote<device::mojom::UsbDevice> device,
                       GetDeviceIdCallback cb,
                       device::mojom::UsbTransferStatus status,
                       base::span<const uint8_t> data) {
  if (status != device::mojom::UsbTransferStatus::COMPLETED || data.empty()) {
    return std::move(cb).Run({});
  }

  // Cleanup device.
  device->ReleaseInterface(kDefaultInterface, base::DoNothing());
  device->Close(base::DoNothing());

  return std::move(cb).Run(chromeos::UsbPrinterId(data));
}

// Callback for device.mojom.UsbDevice.ClaimInterface.
// If interface was claimed successfully, attempts to query printer for a
// Device ID.
void OnClaimInterface(mojo::Remote<device::mojom::UsbDevice> device,
                      GetDeviceIdCallback cb,
                      device::mojom::UsbClaimInterfaceResult result) {
  if (result != device::mojom::UsbClaimInterfaceResult::kSuccess) {
    return std::move(cb).Run({});
  }

  auto params = device::mojom::UsbControlTransferParams::New();
  params->type = device::mojom::UsbControlTransferType::CLASS;
  params->recipient = device::mojom::UsbControlTransferRecipient::INTERFACE;
  params->request = kGetDeviceIdRequest;
  params->value = kDefaultConfiguration;  // default config index
  params->index = kDefaultInterface;      // default interface index

  // Query for IEEE1284 string.
  auto* device_raw = device.get();
  device_raw->ControlTransferIn(
      std::move(params), 255 /* max size */, 2000 /* 2 second timeout */,
      base::BindOnce(OnControlTransfer, std::move(device), std::move(cb)));
}

// Callback for device.mojom.UsbDevice.Open.
// If device was opened successfully, attempts to claim printer's default
// interface.
void OnDeviceOpen(mojo::Remote<device::mojom::UsbDevice> device,
                  GetDeviceIdCallback cb,
                  device::mojom::UsbOpenDeviceResultPtr result) {
  if (result->is_error() || !device) {
    return std::move(cb).Run({});
  }

  // Claim interface.
  auto* device_raw = device.get();
  device_raw->ClaimInterface(
      kDefaultInterface,
      base::BindOnce(OnClaimInterface, std::move(device), std::move(cb)));
}

// Incorporate the bytes of |val| into the incremental hash carried in |ctx| in
// big-endian order.  |val| must be a simple integer type
void MD5UpdateU8BigEndian(base::MD5Context* ctx,
                          base::StrictNumeric<uint8_t> val) {
  uint8_t tmp = val;
  base::MD5Update(ctx, base::span_from_ref(tmp));
}
void MD5UpdateU16BigEndian(base::MD5Context* ctx,
                           base::StrictNumeric<uint16_t> val) {
  base::MD5Update(ctx, base::U16ToBigEndian(val));
}

// Update the hash with the contents of |str|.
//
// UTF-16 strings are a bit fraught for consistency in memory representation;
// endianness is an issue, but more importantly, there are *optional* prefix
// codepoints to identify the endianness of the string.
//
// This is a long way to say "UTF-16 is hard to hash, let's just convert
// to UTF-8 and hash that", which avoids all of these issues.
void MD5UpdateString16(base::MD5Context* ctx, const std::u16string& str) {
  base::MD5Update(ctx, base::UTF16ToUTF8(str));
}

// Get the usb printer id for |device|.  This is used both as the identifier for
// the printer in the user's PrintersManager and as the name of the printer in
// CUPS, so it has to satisfy the naming restrictions of both.  CUPS in
// particular is intolerant of much more than [a-z0-9_-], so we use that
// character set.  This needs to be stable for a given device, but as unique as
// possible for that device.  So we basically toss every bit of stable
// information from the device into an MD5 hash, and then hexify the hash value
// as a suffix to "usb-" as the final printer id.
std::string CreateUsbPrinterId(const UsbDeviceInfo& device_info) {
  // Paranoid checks; in the unlikely event someone messes with the USB device
  // definition, our (supposedly stable) hashes will change.
  static_assert(sizeof(device_info.class_code) == 1, "Class size changed");
  static_assert(sizeof(device_info.subclass_code) == 1,
                "Subclass size changed");
  static_assert(sizeof(device_info.protocol_code) == 1,
                "Protocol size changed");
  static_assert(sizeof(device_info.vendor_id) == 2, "Vendor id size changed");
  static_assert(sizeof(device_info.product_id) == 2, "Product id size changed");
  static_assert(sizeof(device::GetDeviceVersion(device_info)) == 2,
                "Version size changed");

  base::MD5Context ctx;
  base::MD5Init(&ctx);
  MD5UpdateU8BigEndian(&ctx, device_info.class_code);
  MD5UpdateU8BigEndian(&ctx, device_info.subclass_code);
  MD5UpdateU8BigEndian(&ctx, device_info.protocol_code);
  MD5UpdateU16BigEndian(&ctx, device_info.vendor_id);
  MD5UpdateU16BigEndian(&ctx, device_info.product_id);
  MD5UpdateU16BigEndian(&ctx, device::GetDeviceVersion(device_info));
  base::MD5Update(&ctx, GetManufacturerName(device_info));
  base::MD5Update(&ctx, GetProductName(device_info));
  MD5UpdateString16(&ctx, GetSerialNumber(device_info));
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  return base::StringPrintf("usb-%s", base::MD5DigestToBase16(digest).c_str());
}

// Creates a mojom filter which can be used to identify a basic USB printer.
mojo::StructPtr<device::mojom::UsbDeviceFilter> CreatePrinterFilter() {
  auto printer_filter = device::mojom::UsbDeviceFilter::New();
  printer_filter->has_class_code = true;
  printer_filter->class_code = kPrinterInterfaceClass;
  printer_filter->has_subclass_code = true;
  printer_filter->subclass_code = kPrinterInterfaceSubclass;

  return printer_filter;
}

bool UsbDeviceSupportsIppusb(const UsbDeviceInfo& device_info) {
  auto printer_filter = CreatePrinterFilter();
  printer_filter->has_protocol_code = true;
  printer_filter->protocol_code = kPrinterIppusbProtocol;

  return device::UsbDeviceFilterMatches(*printer_filter, device_info);
}

// Convert the interesting details of a device to a string, for
// logging/debugging.
std::string UsbPrinterDeviceDetailsAsString(const UsbDeviceInfo& device_info) {
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
      device_info.guid.c_str(), device::GetUsbVersion(device_info),
      device_info.class_code, device_info.subclass_code,
      device_info.protocol_code, device_info.vendor_id, device_info.product_id,
      device::GetDeviceVersion(device_info),
      GetManufacturerName(device_info).c_str(),
      GetProductName(device_info).c_str(),
      base::UTF16ToUTF8(GetSerialNumber(device_info)).c_str());
}

// Gets the URI CUPS would use to refer to this USB device.  Assumes device
// is a printer.
chromeos::Uri UsbPrinterUri(const UsbDeviceInfo& device_info) {
  // Note that serial may, for some devices, be empty or bogus (all zeros, non
  // unique, or otherwise not really a serial number), but having a non-unique
  // or empty serial field in the URI still lets us print, it just means we
  // don't have a way to uniquely identify a printer if there are multiple ones
  // plugged in with the same VID/PID, so we may print to the *wrong* printer.
  // There doesn't seem to be a robust solution to this problem; if printers
  // don't supply a serial number, we don't have any reliable way to do that
  // differentiation.
  std::string serial = base::UTF16ToUTF8(GetSerialNumber(device_info));
  chromeos::Uri uri;
  uri.SetScheme("usb");
  uri.SetHost(base::StringPrintf("%04x", device_info.vendor_id));
  uri.SetPath({base::StringPrintf("%04x", device_info.product_id)});
  uri.SetQuery({{"serial", serial}});
  return uri;
}

}  // namespace

std::string GuessEffectiveMakeAndModel(
    const device::mojom::UsbDeviceInfo& device_info) {
  return base::StrCat(
      {GetManufacturerName(device_info), " ", GetProductName(device_info)});
}

std::string GetManufacturerName(const UsbDeviceInfo& device_info) {
  return base::UTF16ToUTF8(
      device_info.manufacturer_name.value_or(std::u16string()));
}

std::string GetProductName(const UsbDeviceInfo& device_info) {
  const std::string manufacturer =
      base::StrCat({GetManufacturerName(device_info), " "});
  std::string model =
      base::UTF16ToUTF8(device_info.product_name.value_or(std::u16string()));

  // Some devices have the manufacturer duplicated in the product
  // string. This needs to be removed to remain consistent with the make
  // and model strings in the ppd index.
  if (base::StartsWith(model, manufacturer,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    model.erase(0, manufacturer.size());
  }
  return model;
}

std::u16string GetSerialNumber(const UsbDeviceInfo& device_info) {
  // If the device does not have a serial number or has an empty serial number,
  // use '?' so this matches the convention that CUPS uses for 'no serial
  // number'.
  if (!device_info.serial_number.has_value() ||
      device_info.serial_number.value().empty()) {
    return u"?";
  }
  return device_info.serial_number.value();
}

bool UsbDeviceIsPrinter(const UsbDeviceInfo& device_info) {
  auto printer_filter = CreatePrinterFilter();
  return device::UsbDeviceFilterMatches(*printer_filter, device_info);
}

// Attempt to gather all the information we need to work with this printer by
// querying the USB device.  This should only be called using devices we believe
// are printers, not arbitrary USB devices, as we may get weird partial results
// from arbitrary devices. The results are saved in the second parameter.
bool UsbDeviceToPrinter(const UsbDeviceInfo& device_info,
                        PrinterDetector::DetectedPrinter* entry) {
  DCHECK(entry);

  // Preflight all required fields and log errors if we find something wrong.
  if (device_info.vendor_id == 0 || device_info.product_id == 0) {
    LOG(ERROR) << "Failed to convert USB device to printer.  Fields were:\n"
               << UsbPrinterDeviceDetailsAsString(device_info);
    return false;
  }

  entry->ppd_search_data.usb_manufacturer = GetManufacturerName(device_info);
  entry->ppd_search_data.usb_model = GetProductName(device_info);

  const std::string& make = entry->ppd_search_data.usb_manufacturer;
  const std::string& model = entry->ppd_search_data.usb_model;

  // Synthesize make-and-model string for printer identification.
  entry->printer.set_make_and_model(GuessEffectiveMakeAndModel(device_info));

  entry->printer.set_display_name(MakeDisplayName(make, model));
  entry->printer.set_description(entry->printer.display_name());
  entry->printer.SetUri(UsbPrinterUri(device_info));
  entry->printer.set_id(CreateUsbPrinterId(device_info));
  entry->printer.set_supports_ippusb(UsbDeviceSupportsIppusb(device_info));
  return true;
}

void GetDeviceId(mojo::Remote<device::mojom::UsbDevice> device,
                 GetDeviceIdCallback cb) {
  // Open device.
  auto* device_raw = device.get();
  device_raw->Open(
      base::BindOnce(OnDeviceOpen, std::move(device), std::move(cb)));
}

std::string MakeDisplayName(const std::string& make, const std::string& model) {
  // Construct the display name by however much of the manufacturer/model
  // information that we have available.
  if (make.empty() && model.empty()) {
    return l10n_util::GetStringUTF8(IDS_USB_PRINTER_UNKNOWN_DISPLAY_NAME);
  } else if (!make.empty() && !model.empty()) {
    return l10n_util::GetStringFUTF8(IDS_USB_PRINTER_DISPLAY_NAME,
                                     base::UTF8ToUTF16(make),
                                     base::UTF8ToUTF16(model));
  } else {
    // Exactly one string is present.
    DCHECK_NE(make.empty(), model.empty());
    return l10n_util::GetStringFUTF8(IDS_USB_PRINTER_DISPLAY_NAME_MAKE_OR_MODEL,
                                     base::UTF8ToUTF16(make + model));
  }
}

void UpdateSearchDataFromDeviceId(const chromeos::UsbPrinterId& device_id,
                                  PrinterDetector::DetectedPrinter* printer) {
  // If the IEEE1284 device info looks complete and doesn't match the USB
  // string descriptors, add an additional PPD search string.  In addition, if
  // the USB make_and_model matches known generic strings, replace the entire
  // device description with the values from the IEEE1284 info.
  const std::string& usb_make = device_id.make();
  const std::string& usb_model = device_id.model();
  if (usb_make.empty() || usb_model.empty()) {
    return;
  }
  std::string usb_make_and_model = base::StrCat({usb_make, " ", usb_model});
  if (base::CompareCaseInsensitiveASCII(printer->printer.make_and_model(),
                                        usb_make_and_model) == 0) {
    return;
  }

  if (IsGenericUsbDescription(
          base::ToLowerASCII(printer->printer.make_and_model()))) {
    PRINTER_LOG(EVENT) << printer->printer.make_and_model()
                       << " replaced with USB device info: "
                       << usb_make_and_model;
    printer->printer.set_display_name(MakeDisplayName(usb_make, usb_model));
    printer->printer.set_description(printer->printer.display_name());
    printer->printer.set_make_and_model(usb_make_and_model);
    printer->ppd_search_data.make_and_model.front() =
        base::ToLowerASCII(usb_make_and_model);
  } else {
    // Not a generic string, but still add the IEEE 1284 ID as an additional
    // possible PPD match.
    printer->ppd_search_data.make_and_model.push_back(
        base::ToLowerASCII(usb_make_and_model));
  }
}

}  // namespace ash

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility routines for working with UsbDevices that are printers.

#ifndef CHROME_BROWSER_ASH_PRINTING_USB_PRINTER_UTIL_H_
#define CHROME_BROWSER_ASH_PRINTING_USB_PRINTER_UTIL_H_

#include <optional>
#include <string>

#include "chrome/browser/ash/printing/printer_detector.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_device.mojom-forward.h"

namespace chromeos {
class Printer;
class UsbPrinterId;
}  // namespace chromeos

namespace ash {

// Given a usb device, guesses the make and model for a driver lookup.
std::string GuessEffectiveMakeAndModel(
    const device::mojom::UsbDeviceInfo& device_info);

std::string GetManufacturerName(
    const device::mojom::UsbDeviceInfo& device_info);

std::string GetProductName(const device::mojom::UsbDeviceInfo& device_info);

std::u16string GetSerialNumber(const device::mojom::UsbDeviceInfo& device_info);

bool UsbDeviceIsPrinter(const device::mojom::UsbDeviceInfo& device_info);

// Attempt to gather all the information we need to work with this printer by
// querying the USB device.  This should only be called using devices we believe
// are printers, not arbitrary USB devices, as we may get weird partial results
// from arbitrary devices. The results are saved in the second parameter.
//
// Returns false and logs an error on failure.
bool UsbDeviceToPrinter(const device::mojom::UsbDeviceInfo& device_info,
                        PrinterDetector::DetectedPrinter* entry);

// Expects `device` to be linked to a Printer-class USB Device described by
// `device_info`. Queries the printer for its IEEE 1284 Standard Device ID.
using GetDeviceIdCallback = base::OnceCallback<void(chromeos::UsbPrinterId)>;
void GetDeviceId(const device::mojom::UsbDeviceInfo& device_info,
                 mojo::Remote<device::mojom::UsbDevice> device,
                 GetDeviceIdCallback cb);

// Create a USB printer display name that incorporates any non-empty values from
// |make| and |model|.
std::string MakeDisplayName(const std::string& make, const std::string& model);

// Add device info from `device_id` to the PPD search information in `printer`.
// If `printer` contains generic USB strings, replace the default search data
// from `device_id` entirely.
void UpdateSearchDataFromDeviceId(const chromeos::UsbPrinterId& device_id,
                                  PrinterDetector::DetectedPrinter* printer);

// Implementation details exposed only for testing.
namespace internal {
// Where to send GET_DEVICE_ID class-specific requests.
struct PrinterInterfaceTarget {
  // Device VID:PID for logging.
  std::string vidpid;

  // Zero-based config index.
  uint8_t config;

  // Zero-based interface index.
  uint8_t interface;

  // Zero-based interface alternate.
  uint8_t alternate;

  // True if SET_INTERFACE is needed before sending printer class requests,
  // primarily if the interface has more than one alternate.
  bool set_alternate;

  bool operator==(const PrinterInterfaceTarget&) const = default;
};

// Find a valid printer class interface for sending GET_DEVICE_ID requests.
std::optional<PrinterInterfaceTarget> FindPrinterInterfaceTarget(
    const device::mojom::UsbDeviceInfo& device_info);
}  // namespace internal

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_USB_PRINTER_UTIL_H_

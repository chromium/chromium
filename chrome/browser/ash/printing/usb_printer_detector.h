// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_USB_PRINTER_DETECTOR_H_
#define CHROME_BROWSER_ASH_PRINTING_USB_PRINTER_DETECTOR_H_

#include <memory>

#include "chrome/browser/ash/printing/printer_detector.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/usb_manager.mojom-forward.h"

namespace ash {

class SessionController;

// Observes device::UsbService for addition of USB printers, and implements the
// PrinterDetector interface to export this to print system consumers.
class UsbPrinterDetector : public PrinterDetector {
 public:
  // Factory function for the CUPS implementation.
  static std::unique_ptr<UsbPrinterDetector> Create();

  static std::unique_ptr<UsbPrinterDetector> CreateForTesting(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager,
      ash::SessionController* session_controller);

  UsbPrinterDetector(const UsbPrinterDetector&) = delete;
  UsbPrinterDetector& operator=(const UsbPrinterDetector&) = delete;

  ~UsbPrinterDetector() override = default;

 protected:
  UsbPrinterDetector() = default;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_USB_PRINTER_DETECTOR_H_

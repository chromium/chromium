// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_PRINTING_PRINTER_INSTALLATION_MANAGER_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINTER_INSTALLATION_MANAGER_H_

#include "chrome/browser/ash/printing/printer_configurer.h"

namespace chromeos {
class Printer;
}

namespace ash {

// Interface that exposes methods for tracking the installation of a printer.
class PrinterInstallationManager {
 public:
  virtual ~PrinterInstallationManager() = default;

  // Record that the given printers has been installed in CUPS for usage.
  // Parameter |is_automatic| should be set to true if the printer was
  // saved automatically (without requesting additional information
  // from the user).
  virtual void PrinterInstalled(const chromeos::Printer& printer,
                                bool is_automatic) = 0;

  // Returns true if |printer| is currently installed in CUPS with this
  // configuration.
  virtual bool IsPrinterInstalled(const chromeos::Printer& printer) const = 0;

  // Record that a requested printer installation failed because the printer
  // is not autoconfigurable (does not meet IPP Everywhere requirements).
  // This results in shifting the printer from automatic to discovered class.
  virtual void PrinterIsNotAutoconfigurable(
      const chromeos::Printer& printer) = 0;

  // Install `printer` in CUPS. The result is returned by `callback`.
  virtual void SetUpPrinter(const chromeos::Printer& printer,
                            PrinterSetupCallback callback) = 0;
  // Remove `printer` from CUPS.
  virtual void UninstallPrinter(const std::string& printer_id) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINTER_INSTALLATION_MANAGER_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINTER_CONFIGURER_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINTER_CONFIGURER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "url/gurl.h"

class Profile;

namespace chromeos {
class Printer;
}

namespace ash {

// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum PrinterSetupResult {
  kFatalError = 0,                // Setup failed in an unrecognized way
  kSuccess = 1,                   // Printer set up successfully
  kPrinterUnreachable = 2,        // Could not reach printer
  kDbusError = 3,                 // Could not contact debugd
  kNativePrintersNotAllowed = 4,  // Tried adding/editing printers policy set
  kInvalidPrinterUpdate = 5,      // Tried updating printer with invalid values
  kComponentUnavailable = 6,      // Could not install component
  kEditSuccess = 7,               // Printer edited successfully
  kPrinterSentWrongResponse = 8,  // Printer sent unexpected response
  kPrinterIsNotAutoconfigurable = 9,  // Printer requires PPD

  // PPD errors
  kPpdTooLarge = 10,       // PPD exceeds size limit
  kInvalidPpd = 11,        // PPD rejected by cupstestppd
  kPpdNotFound = 12,       // Could not find PPD
  kPpdUnretrievable = 13,  // Could not download PPD

  // Other errors
  kIoError = 14,                // I/O error in CUPS
  kMemoryAllocationError = 15,  // Memory allocation error in Cups
  kBadUri = 16,                 // Printer's URI is incorrect
  kManualSetupRequired = 17,    // Printer requires manual setup
  // Space left for additional errors

  // Specific DBus errors. This must stay in sync with the DBusLibraryError
  // enum and PrinterSetupResultFromDbusErrorCode().
  kDbusNoReply = 64,  // Expected remote response but got nothing
  kDbusTimeout = 65,  // Generic timeout error (c.f. dbus-protocol.h)
  kMaxValue           // Maximum value for histograms
};

// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
// Records the source of a successful USB printer setup.
enum class UsbPrinterSetupSource {
  kSettings = 0,        // USB printer installed via Settings.
  kPrintPreview = 1,    // USB printer installed via Print Preview.
  kAutoconfigured = 2,  // USB printer installed automatically.
  kMaxValue = kAutoconfigured,
};

using PrinterSetupCallback = base::OnceCallback<void(PrinterSetupResult)>;

// Configures printers by retrieving PPDs and registering the printer with CUPS.
// Class must be constructed and used on the UI thread.
class PrinterConfigurer {
 public:
  static std::unique_ptr<PrinterConfigurer> Create(Profile* profile);

  PrinterConfigurer(const PrinterConfigurer&) = delete;
  PrinterConfigurer& operator=(const PrinterConfigurer&) = delete;

  virtual ~PrinterConfigurer() = default;

  // Set up |printer| retrieving the appropriate PPD and registering the printer
  // with CUPS.  |callback| is called with the result of the operation.  This
  // method must be called on the UI thread and will run |callback| on the
  // UI thread.
  virtual void SetUpPrinter(const chromeos::Printer& printer,
                            PrinterSetupCallback callback) = 0;

  // Return an opaque fingerprint of the fields used to set up a printer with
  // CUPS.  The idea here is that if this fingerprint changes for a printer, we
  // need to reconfigure CUPS.  This fingerprint is not guaranteed to be stable
  // across reboots.
  static std::string SetupFingerprint(const chromeos::Printer& printer);

  // Records UMA metrics for USB printer setup.
  static void RecordUsbPrinterSetupSource(UsbPrinterSetupSource source);

  // Test method to override the printer configurer for testing.
  static void SetPrinterConfigurerForTesting(
      std::unique_ptr<PrinterConfigurer> printer_configurer);

  // Returns a generated EULA GURL for the provided |license|. |license| is the
  // identifier tag of the printer's license information.
  static GURL GeneratePrinterEulaUrl(const std::string& license);

 protected:
  PrinterConfigurer() = default;
};

// Return a message for |result| that can be used in device-log.
std::string ResultCodeToMessage(const PrinterSetupResult result);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINTER_CONFIGURER_H_

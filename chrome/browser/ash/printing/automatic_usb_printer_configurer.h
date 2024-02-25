// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_AUTOMATIC_USB_PRINTER_CONFIGURER_H_
#define CHROME_BROWSER_ASH_PRINTING_AUTOMATIC_USB_PRINTER_CONFIGURER_H_

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/printing/printer_detector.h"
#include "chrome/browser/ash/printing/printer_installation_manager.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"

namespace ash {

class UsbPrinterNotificationController;

// This class is responsible for automatic setup of USB printers. The list of
// all currently connected USB printers should be provided by calling a method
// UpdateListOfConnectedPrinters(). This method calculates the difference
// between the given list of printers and the list passed in the previous call.
// Printers that disappeared from the list are uninstalled. For each new printer
// on the list, an installation procedure is started. Started installation
// procedures run asynchronously and independently for every printer. When a
// procedure for given printer finishes, the printer is assigned to the list
// of configured printers (when the setup succeeded) or to the list of
// unconfigured printers (when the automatic setup failed). After that, the
// callback `refresh_callback` (passed to the constructor) is called with the
// printer's id as a parameter.
class AutomaticUsbPrinterConfigurer {
 public:
  // The parameters must not be null. `refresh_callback` is called every time a
  // new printer is added to a list of configured or unconfigured printers and
  // it takes a printer's id as a parameter.
  AutomaticUsbPrinterConfigurer(
      PrinterInstallationManager* installation_manager,
      UsbPrinterNotificationController* notification_controller,
      chromeos::PpdProvider* ppd_provider,
      base::RepeatingCallback<void(std::string)> refresh_callback);

  AutomaticUsbPrinterConfigurer(const AutomaticUsbPrinterConfigurer&) = delete;
  AutomaticUsbPrinterConfigurer& operator=(
      const AutomaticUsbPrinterConfigurer&) = delete;
  ~AutomaticUsbPrinterConfigurer();

  // `new_list` must contain all USB printers connected currently to the device.
  // For each added or removed printer (comparing to the previous call of this
  // method) an installation or uninstallation procedure is started,
  // respectively.
  void UpdateListOfConnectedPrinters(
      std::vector<PrinterDetector::DetectedPrinter> new_list);

  // Return the current list of configured or unconfigured printers.
  const base::flat_set<std::string>& ConfiguredPrintersIds() const {
    return configured_printers_;
  }
  const base::flat_set<std::string>& UnconfiguredPrintersIds() const {
    return unconfigured_printers_;
  }

  // Return the object Printer for given `printer_id`. This method can be
  // called ONLY for the printers included in the last call of
  // UpdateListOfConnectedPrinters(). The returned object may be different than
  // the original one sent in UpdateListOfConnectedPrinters(), but the printer
  // id never changes.
  const chromeos::Printer& Printer(const std::string& printer_id) const {
    DCHECK(base::Contains(connected_printers_, printer_id));
    return connected_printers_.at(printer_id).printer;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(AutomaticUsbPrinterConfigurerTest,
                           UsbPrinterAddedToSet);

  // Uses |printer_configurer_| to setup |printer| if it is not yet setup.
  void ConfigurePrinter(const std::string& printer_id);

  // Callback for PpdProvider::ResolvePpdReference().
  void OnResolvePpdReferenceDone(const std::string& printer_id,
                                 chromeos::PpdProvider::CallbackResultCode code,
                                 const chromeos::Printer::PpdReference& ref,
                                 const std::string& usb_manufacturer);

  // Callback for PrinterConfiguer::SetUpPrinter().
  void OnSetupComplete(const chromeos::Printer& printer,
                       PrinterSetupResult result);

  // Update list of configured or unconfigured printers, show notification and
  // call `refresh_callback`.
  void FinalizeConfiguration(const chromeos::Printer& printer, bool success);

  SEQUENCE_CHECKER(sequence_);

  raw_ptr<PrinterInstallationManager> installation_manager_;  // Not owned.
  raw_ptr<UsbPrinterNotificationController>
      notification_controller_;                  // Not owned.
  raw_ptr<chromeos::PpdProvider> ppd_provider_;  // Not owned.
  base::RepeatingCallback<void(std::string)> refresh_callback_;

  base::flat_map<std::string, PrinterDetector::DetectedPrinter>
      connected_printers_;
  base::flat_set<std::string> pending_ppd_resolutions_;
  base::flat_map<std::string, chromeos::Printer::PpdReference> ppd_references_;
  base::flat_set<std::string> configured_printers_;
  base::flat_set<std::string> unconfigured_printers_;

  base::WeakPtrFactory<AutomaticUsbPrinterConfigurer> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_AUTOMATIC_USB_PRINTER_CONFIGURER_H_

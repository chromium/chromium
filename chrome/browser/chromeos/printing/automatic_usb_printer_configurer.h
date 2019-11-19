// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_AUTOMATIC_USB_PRINTER_CONFIGURER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_AUTOMATIC_USB_PRINTER_CONFIGURER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/printer_installation_manager.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

class UsbPrinterNotificationController;

class AutomaticUsbPrinterConfigurer : public CupsPrintersManager::Observer {
 public:
  AutomaticUsbPrinterConfigurer(
      std::unique_ptr<PrinterConfigurer> printer_configurer,
      PrinterInstallationManager* installation_manager,
      UsbPrinterNotificationController* notification_controller);

  ~AutomaticUsbPrinterConfigurer() override;

  // CupsPrintersManager::Observer override.
  void OnPrintersChanged(PrinterClass printer_class,
                         const std::vector<Printer>& printers) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AutomaticUsbPrinterConfigurerTest,
                           UsbPrinterAddedToSet);

  // Uses |printer_configurer_| to setup |printer| if it is not yet setup.
  void SetupPrinter(const Printer& printer);

  // Callback for PrinterConfiguer::SetUpPrinter().
  void OnSetupComplete(const Printer& printer, PrinterSetupResult result);

  // Completes the configuration for |printer|. Saves printer in
  // |configured_printers_|.
  void CompleteConfiguration(const Printer& printer);

  // Removes any printers from |configured_printers_| that are no longer in
  // |automatic_printers|.
  void PruneRemovedAutomaticPrinters(
      const std::vector<Printer>& automatic_printers);

  // Removes any printers from |unconfigured_printers_| that are no longer in
  // |discovered_printers|.
  void PruneRemovedDiscoveredPrinters(
      const std::vector<Printer>& discovered_printers);

  // Helper function that removes printers that are no longer in
  // |current_printers|. If |use_configured_printers|, |configured_printers_| is
  // pruned. Otherwise, |unconfigured_printers_| is pruned.
  void PruneRemovedPrinters(const std::vector<Printer>& current_printers,
                            bool use_configured_printers);

  SEQUENCE_CHECKER(sequence_);

  std::unique_ptr<PrinterConfigurer> printer_configurer_;
  PrinterInstallationManager* installation_manager_;  // Not owned.
  UsbPrinterNotificationController* notification_controller_;  // Not owned.
  base::flat_set<std::string> configured_printers_;
  base::flat_set<std::string> unconfigured_printers_;

  base::WeakPtrFactory<AutomaticUsbPrinterConfigurer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AutomaticUsbPrinterConfigurer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_AUTOMATIC_USB_PRINTER_CONFIGURER_H_

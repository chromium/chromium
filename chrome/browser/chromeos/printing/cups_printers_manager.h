// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINTERS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINTERS_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/printing/printer_installation_manager.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {

class PpdProvider;
class PrinterConfigurer;
class PrinterDetector;
class PrinterEventTracker;
class ServerPrintersProvider;
class SyncedPrintersManager;
class UsbPrinterNotificationController;

enum class PrinterSetupSource;

// Top level manager of available CUPS printers in ChromeOS.  All functions
// in this class must be called from a sequenced context.
class CupsPrintersManager : public PrinterInstallationManager,
                            public KeyedService {
 public:
  class Observer {
   public:
    // The list of printers in this class has changed to the given printers.
    virtual void OnPrintersChanged(PrinterClass printer_class,
                                   const std::vector<Printer>& printers) {}
    // It is called exactly once for each observer. It means that the
    // subsystem for enterprise printers is initialized. When an observer is
    // being registered after the subsystem's initialization, this call is
    // scheduled immediately in AddObserver method.
    virtual void OnEnterprisePrintersInitialized() {}

    virtual ~Observer() = default;
  };

  // Factory function.
  static std::unique_ptr<CupsPrintersManager> Create(Profile* profile);

  // Factory function that allows injected dependencies, for testing.  Ownership
  // is not taken of any of the raw-pointer arguments.
  static std::unique_ptr<CupsPrintersManager> CreateForTesting(
      SyncedPrintersManager* synced_printers_manager,
      std::unique_ptr<PrinterDetector> usb_printer_detector,
      std::unique_ptr<PrinterDetector> zeroconf_printer_detector,
      scoped_refptr<PpdProvider> ppd_provider,
      std::unique_ptr<PrinterConfigurer> printer_configurer,
      std::unique_ptr<UsbPrinterNotificationController>
          usb_notification_controller,
      std::unique_ptr<ServerPrintersProvider> server_printers_provider,
      PrinterEventTracker* event_tracker,
      PrefService* pref_service);

  // Register the printing preferences with the |registry|.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  ~CupsPrintersManager() override = default;

  // Get the known printers in the given class.
  virtual std::vector<Printer> GetPrinters(
      PrinterClass printer_class) const = 0;

  // Saves |printer|. If |printer| already exists in the saved class, it will
  // be overwritten.
  virtual void SavePrinter(const Printer& printer) = 0;

  // Remove the saved printer with the given id.  This is a NOP if
  // the printer_id is not that of a saved printer.
  virtual void RemoveSavedPrinter(const std::string& printer_id) = 0;

  // Add or remove observers.  Observers must be on the same
  // sequence as the CupsPrintersManager.  Callbacks for a given observer
  // will be on the same sequence as the CupsPrintersManager.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Record that the given printers has been installed in CUPS for usage.
  // Parameter |is_automatic| should be set to true if the printer was
  // saved automatically (without requesting additional information
  // from the user).
  void PrinterInstalled(const Printer& printer,
                        bool is_automatic,
                        PrinterSetupSource source) override = 0;

  // Returns true if |printer| is currently installed in CUPS with this
  // configuration.
  bool IsPrinterInstalled(const Printer& printer) const override = 0;

  // Look for a printer with the given id in any class.  Returns a copy of the
  // printer if found, base::nullopt if not found.
  virtual base::Optional<Printer> GetPrinter(const std::string& id) const = 0;

  // Log an event that the user started trying to set up the given printer,
  // but setup was not completed for some reason.
  virtual void RecordSetupAbandoned(const Printer& printer) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINTERS_MANAGER_H_

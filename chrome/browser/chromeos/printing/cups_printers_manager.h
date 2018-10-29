// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINTERS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINTERS_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/printing/printer_event_tracker.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {

class PpdProvider;
class PrinterDetector;
class PrinterEventTracker;
class SyncedPrintersManager;

// Top level manager of available CUPS printers in ChromeOS.  All functions
// in this class must be called from a sequenced context.
class CupsPrintersManager : public KeyedService {
 public:
  // Classes of printers tracked.  See doc/cups_printer_management.md for
  // details on what these mean.
  enum PrinterClass {
    kConfigured,
    kEnterprise,
    kAutomatic,
    kDiscovered,
    kNumPrinterClasses
  };

  class Observer {
   public:
    // The list of printers in this class has changed to the given printers.
    virtual void OnPrintersChanged(PrinterClass printer_class,
                                   const std::vector<Printer>& printers) = 0;

   protected:
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
      PrinterEventTracker* event_tracker,
      PrefService* pref_service);

  // Register the printing preferences with the |registry|.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  ~CupsPrintersManager() override = default;

  // Get the known printers in the given class.
  virtual std::vector<Printer> GetPrinters(
      PrinterClass printer_class) const = 0;

  // Remove any printer from printers that we know we cannot currently
  // talk to.  Examples would be USB printers that are not currently
  // plugged in, or Zeroconf printers that have not been detected this
  // session.
  virtual void RemoveUnavailablePrinters(
      std::vector<Printer>* printers) const = 0;

  // Update or save a printer as a configured printer.  If this is the same as
  // an existing configured printer, the entry will be updated.  If the printer
  // appears in a class other than configured, it will be moved to the
  // configured class.
  virtual void UpdateConfiguredPrinter(const Printer& printer) = 0;

  // Remove the configured printer with the given id.  This is a NOP if
  // the printer_id is not that of a configured printer.
  virtual void RemoveConfiguredPrinter(const std::string& printer_id) = 0;

  // Add or remove observers.  Observers do not need to be on the same
  // sequence as the CupsPrintersManager.  Callbacks for a given observer
  // will be on the same sequence as was used to call AddObserver().
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Record that the given printers has been installed in CUPS for usage.  If
  // |printer| is not a configured or enterprise printer, this will have the
  // side effect of moving |printer| into the configured class.
  // Parameter |is_automatic| should be set to true if the printer was
  // configured automatically (without requesting additional information
  // from the user).
  virtual void PrinterInstalled(const Printer& printer, bool is_automatic) = 0;

  // Returns true if |printer| is currently installed in CUPS with this
  // configuration.
  virtual bool IsPrinterInstalled(const Printer& printer) const = 0;

  // Look for a printer with the given id in any class.  Returns a copy of the
  // printer if found, null if not found.
  virtual std::unique_ptr<Printer> GetPrinter(const std::string& id) const = 0;

  // Log an event that the user started trying to set up the given printer,
  // but setup was not completed for some reason.
  virtual void RecordSetupAbandoned(const Printer& printer) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINTERS_MANAGER_H_

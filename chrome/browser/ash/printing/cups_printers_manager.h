// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PRINTERS_MANAGER_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PRINTERS_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "chrome/browser/ash/printing/print_servers_manager.h"
#include "chrome/browser/ash/printing/printer_configurer.h"
#include "chrome/browser/ash/printing/printer_installation_manager.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/uri.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace chromeos {
class PpdProvider;
}

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash {

class DlcserviceClient;
class EnterprisePrintersProvider;
class PrinterDetector;
class PrinterEventTracker;
class SyncedPrintersManager;
class UsbPrinterNotificationController;

// Returns true if |printer_uri| is an IPP uri.
bool IsIppUri(const chromeos::Uri& printer_uri);

// Top level manager of available CUPS printers in ChromeOS.  All functions
// in this class must be called from a sequenced context.
class CupsPrintersManager : public PrinterInstallationManager,
                            public KeyedService {
 public:
  class Observer {
   public:
    // The list of printers in this class has changed to the given printers.
    virtual void OnPrintersChanged(
        chromeos::PrinterClass printer_class,
        const std::vector<chromeos::Printer>& printers) {}
    // It is called exactly once for each observer. It means that the
    // subsystem for enterprise printers is initialized. When an observer is
    // being registered after the subsystem's initialization, this call is
    // scheduled immediately in AddObserver method.
    virtual void OnEnterprisePrintersInitialized() {}

    virtual ~Observer() = default;
  };

  class LocalPrintersObserver : public base::CheckedObserver {
   public:
    // This endpoint is only triggered for the following scenarios:
    //   1. A new local printer is either plugged in or detected on the network.
    //   2. A local printer receives an updated printer status.
    virtual void OnLocalPrintersUpdated() {}

   protected:
    ~LocalPrintersObserver() override = default;
  };

  using PrinterStatusCallback =
      base::OnceCallback<void(const chromeos::CupsPrinterStatus&)>;

  // Factory function.
  static std::unique_ptr<CupsPrintersManager> Create(Profile* profile);

  // Factory function that allows injected dependencies, for testing.  Ownership
  // is not taken of any of the raw-pointer arguments.
  static std::unique_ptr<CupsPrintersManager> CreateForTesting(
      SyncedPrintersManager* synced_printers_manager,
      std::unique_ptr<PrinterDetector> usb_printer_detector,
      std::unique_ptr<PrinterDetector> zeroconf_printer_detector,
      scoped_refptr<chromeos::PpdProvider> ppd_provider,
      DlcserviceClient* dlc_service_client,
      std::unique_ptr<UsbPrinterNotificationController>
          usb_notification_controller,
      std::unique_ptr<PrintServersManager> print_servers_manager,
      std::unique_ptr<EnterprisePrintersProvider> enterprise_printers_provider,
      PrinterEventTracker* event_tracker,
      PrefService* pref_service);

  // Register the profile printing preferences with the |registry|.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Register the printing preferences with the |registry|.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  ~CupsPrintersManager() override = default;

  // Get the known printers in the given class.
  virtual std::vector<chromeos::Printer> GetPrinters(
      chromeos::PrinterClass printer_class) const = 0;

  // Saves |printer|. If |printer| already exists in the saved class, it will
  // be overwritten. This is a NOP if |printer| is an enterprise or USB printer.
  virtual void SavePrinter(const chromeos::Printer& printer) = 0;

  // Remove the saved printer with the given id.  This is a NOP if
  // the printer_id is not that of a saved printer.
  virtual void RemoveSavedPrinter(const std::string& printer_id) = 0;

  // Add or remove observers.  Observers must be on the same
  // sequence as the CupsPrintersManager.  Callbacks for a given observer
  // will be on the same sequence as the CupsPrintersManager.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual void AddLocalPrintersObserver(LocalPrintersObserver* observer) = 0;
  virtual void RemoveLocalPrintersObserver(LocalPrintersObserver* observer) = 0;

  // Implementation of PrinterInstallationManager interface.
  bool IsPrinterInstalled(const chromeos::Printer& printer) const override = 0;
  void SetUpPrinter(const chromeos::Printer& printer,
                    bool is_automatic_installation,
                    PrinterSetupCallback callback) override = 0;
  void UninstallPrinter(const std::string& printer_id) override = 0;

  // Look for a printer with the given id in any class.  Returns a copy of the
  // printer if found, std::nullopt if not found.
  virtual std::optional<chromeos::Printer> GetPrinter(
      const std::string& id) const = 0;

  // Log an event that the user started trying to set up the given printer,
  // but setup was not completed for some reason.
  virtual void RecordSetupAbandoned(const chromeos::Printer& printer) = 0;

  // Performs individual printer status requests for each printer provided.
  // Passes retrieved printer status to the callbacks.
  virtual void FetchPrinterStatus(const std::string& printer_id,
                                  PrinterStatusCallback cb) = 0;

  // Records the total number of detected network printers and the
  // number of detected network printers that have not been saved.
  virtual void RecordNearbyNetworkPrinterCounts() const = 0;

  virtual PrintServersManager* GetPrintServersManager() const = 0;

  // Performs an IPP query on `printer` for autoconf compatibility.
  virtual void QueryPrinterForAutoConf(
      const chromeos::Printer& printer,
      base::OnceCallback<void(bool)> callback) = 0;
};

}  // namespace ash

namespace base {

template <>
struct ScopedObservationTraits<
    ash::CupsPrintersManager,
    ash::CupsPrintersManager::LocalPrintersObserver> {
  static void AddObserver(
      ash::CupsPrintersManager* source,
      ash::CupsPrintersManager::LocalPrintersObserver* observer) {
    source->AddLocalPrintersObserver(observer);
  }
  static void RemoveObserver(
      ash::CupsPrintersManager* source,
      ash::CupsPrintersManager::LocalPrintersObserver* observer) {
    source->RemoveLocalPrintersObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PRINTERS_MANAGER_H_

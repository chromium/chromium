// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_SYNCED_PRINTERS_MANAGER_H_
#define CHROME_BROWSER_ASH_PRINTING_SYNCED_PRINTERS_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {

class PrintersSyncBridge;

// Manages information about synced local printers classes (SAVED
// and ENTERPRISE).  Provides an interface to a user's printers and
// printers provided by policy.  User printers are backed by the
// PrintersSyncBridge.
//
// This class is thread-safe.
// TODO(crbug/1030127): Rename to SavedPrintersManager & remove KeyedService.
// TODO(crbug/1030127): Remove lock and add a sequence_checker.
class SyncedPrintersManager : public KeyedService {
 public:
  class Observer {
   public:
    virtual void OnSavedPrintersChanged() = 0;
  };

  static std::unique_ptr<SyncedPrintersManager> Create(
      std::unique_ptr<PrintersSyncBridge> sync_bridge);
  ~SyncedPrintersManager() override = default;

  // Returns the printers that are saved in preferences.
  virtual std::vector<chromeos::Printer> GetSavedPrinters() const = 0;

  // Returns the printer with id |printer_id|, or nullptr if no such printer
  // exists.  Searches both Saved and Enterprise printers.
  virtual std::unique_ptr<chromeos::Printer> GetPrinter(
      const std::string& printer_id) const = 0;

  // Updates a printer in profile preferences.  The |printer| is
  // identified by its id field. If |printer| is *not* a saved printer,
  // |printer| will become a saved printer.
  virtual void UpdateSavedPrinter(const chromeos::Printer& printer) = 0;

  // Remove printer from preferences with the id |printer_id|.  Returns true if
  // the printer was successfully removed.
  virtual bool RemoveSavedPrinter(const std::string& printer_id) = 0;

  // Attach |observer| for notification of events.  |observer| is expected to
  // live on the same thread (UI) as this object.  OnPrinter* methods are
  // invoked inline so calling RegisterPrinter in response to OnPrinterAdded is
  // forbidden.
  virtual void AddObserver(SyncedPrintersManager::Observer* observer) = 0;

  // Remove |observer| so that it no longer receives notifications.  After the
  // completion of this method, the |observer| can be safely destroyed.
  virtual void RemoveObserver(SyncedPrintersManager::Observer* observer) = 0;

  // Returns a DataTypeSyncBridge for the sync client.
  virtual PrintersSyncBridge* GetSyncBridge() = 0;

};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_SYNCED_PRINTERS_MANAGER_H_

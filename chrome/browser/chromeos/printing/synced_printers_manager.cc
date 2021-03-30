// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/synced_printers_manager.h"

#include <unordered_map>
#include <utility>

#include "base/guid.h"
#include "base/observer_list_threadsafe.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/printing/enterprise_printers_provider.h"
#include "chrome/browser/chromeos/printing/printers_sync_bridge.h"
#include "chrome/browser/chromeos/printing/specifics_translation.h"
#include "chrome/common/pref_names.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

class SyncedPrintersManagerImpl : public SyncedPrintersManager,
                                  public PrintersSyncBridge::Observer {
 public:
  explicit SyncedPrintersManagerImpl(
      std::unique_ptr<PrintersSyncBridge> sync_bridge)
      : sync_bridge_(std::move(sync_bridge)),
        observers_(new base::ObserverListThreadSafe<
                   SyncedPrintersManager::Observer>()) {
    sync_bridge_->AddObserver(this);
  }

  ~SyncedPrintersManagerImpl() override {
    sync_bridge_->RemoveObserver(this);
  }

  std::vector<Printer> GetSavedPrinters() const override {
    // No need to lock here, since sync_bridge_ is thread safe and we don't
    // touch anything else.
    std::vector<Printer> printers;
    std::vector<sync_pb::PrinterSpecifics> values =
        sync_bridge_->GetAllPrinters();
    for (const auto& value : values) {
      printers.push_back(*SpecificsToPrinter(value));
    }
    return printers;
  }

  std::unique_ptr<Printer> GetPrinter(
      const std::string& printer_id) const override {
    base::AutoLock l(lock_);
    return GetPrinterLocked(printer_id);
  }

  void UpdateSavedPrinter(const Printer& printer) override {
    base::AutoLock l(lock_);
    UpdateSavedPrinterLocked(printer);
  }

  bool RemoveSavedPrinter(const std::string& printer_id) override {
    return sync_bridge_->RemovePrinter(printer_id);
  }

  void AddObserver(SyncedPrintersManager::Observer* observer) override {
    observers_->AddObserver(observer);
  }

  void RemoveObserver(SyncedPrintersManager::Observer* observer) override {
    observers_->RemoveObserver(observer);
  }

  PrintersSyncBridge* GetSyncBridge() override { return sync_bridge_.get(); }

  // PrintersSyncBridge::Observer override.
  void OnPrintersUpdated() override {
    observers_->Notify(
        FROM_HERE, &SyncedPrintersManager::Observer::OnSavedPrintersChanged);
  }

 private:
  std::unique_ptr<Printer> GetPrinterLocked(
      const std::string& printer_id) const {
    lock_.AssertAcquired();
    base::Optional<sync_pb::PrinterSpecifics> printer =
        sync_bridge_->GetPrinter(printer_id);
    return printer.has_value() ? SpecificsToPrinter(*printer) : nullptr;
  }

  void UpdateSavedPrinterLocked(const Printer& printer_arg) {
    lock_.AssertAcquired();
    DCHECK_EQ(Printer::SRC_USER_PREFS, printer_arg.source());

    // Need a local copy since we may set the id.
    Printer printer = printer_arg;
    if (printer.id().empty()) {
      printer.set_id(base::GenerateGUID());
    }

    sync_bridge_->UpdatePrinter(PrinterToSpecifics(printer));
  }

  mutable base::Lock lock_;

  // The backend for profile printers.
  std::unique_ptr<PrintersSyncBridge> sync_bridge_;

  scoped_refptr<base::ObserverListThreadSafe<SyncedPrintersManager::Observer>>
      observers_;
  base::WeakPtrFactory<SyncedPrintersManagerImpl> weak_factory_{this};
};

}  // namespace

// static
std::unique_ptr<SyncedPrintersManager> SyncedPrintersManager::Create(
    std::unique_ptr<PrintersSyncBridge> sync_bridge) {
  return std::make_unique<SyncedPrintersManagerImpl>(std::move(sync_bridge));
}

}  // namespace chromeos

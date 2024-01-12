// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_printers_manager_proxy.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chromeos/printing/printer_configuration.h"

namespace ash {

namespace {

class ProxyImpl : public CupsPrintersManagerProxy,
                  public CupsPrintersManager::Observer {
 public:
  ProxyImpl() = default;

  ~ProxyImpl() override {
    // Verify that the active manager has been unset when we're cleaned up.
    DCHECK(active_manager_ == nullptr);
  }

  void AddObserver(CupsPrintersManager::Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(CupsPrintersManager::Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  void SetManager(CupsPrintersManager* manager) override {
    if (active_manager_ != nullptr) {
      DVLOG(1) << "Discarding manager when one is already set.  This should "
                  "only happen during testing.";
      return;
    }

    active_manager_ = manager;
    active_manager_->AddObserver(this);

    // Emit a change event to wake up any observers.
    // Emitting for saved printers is an arbitrary decision.
    OnPrintersChanged(
        chromeos::PrinterClass::kSaved,
        active_manager_->GetPrinters(chromeos::PrinterClass::kSaved));
  }

  void RemoveManager(CupsPrintersManager* manager) override {
    if (!active_manager_) {
      // It's possible no manager was ever attached and we ignore it.
      return;
    }

    if (manager != active_manager_) {
      // Ignore removals of unattached managers.
      return;
    }

    active_manager_->RemoveObserver(this);
    active_manager_ = nullptr;
  }

  // CupsPrintersManager::Observer overrides
  void OnPrintersChanged(
      chromeos::PrinterClass printer_class,
      const std::vector<chromeos::Printer>& printers) override {
    for (auto& observer : observers_) {
      observer.OnPrintersChanged(printer_class, printers);
    }
  }

 private:
  // The manager for which we are forwarding events.
  raw_ptr<CupsPrintersManager> active_manager_ = nullptr;
  // TODO(skau): Change to CheckedObservers
  base::ObserverList<CupsPrintersManager::Observer>::Unchecked observers_;
};

}  // namespace

// static
std::unique_ptr<CupsPrintersManagerProxy> CupsPrintersManagerProxy::Create() {
  return std::make_unique<ProxyImpl>();
}

}  // namespace ash

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_PRINTERS_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_PRINTERS_SERVICE_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager_proxy.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace ash {

// Provides a DBus service which emits a signal to indicate that the available
// printers has changed.  Clients are intended to listen for the signal then
// make a request for more printers through a side channel e.g. cups_proxy.
class PrintersServiceProvider
    : public CrosDBusService::ServiceProviderInterface,
      public CupsPrintersManager::Observer {
 public:
  PrintersServiceProvider();

  PrintersServiceProvider(const PrintersServiceProvider&) = delete;
  PrintersServiceProvider& operator=(const PrintersServiceProvider&) = delete;

  ~PrintersServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

  // CupsPrintersManager::Observer overrides:
  void OnPrintersChanged(
      chromeos::PrinterClass printers_class,
      const std::vector<chromeos::Printer>& printers) override;

 private:
  // Emits the D-Bus signal for this event.
  void EmitSignal();

  // A reference on ExportedObject for sending signals.
  scoped_refptr<dbus::ExportedObject> exported_object_;

  base::ScopedObservation<CupsPrintersManagerProxy,
                          CupsPrintersManager::Observer>
      printers_manager_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_PRINTERS_SERVICE_PROVIDER_H_

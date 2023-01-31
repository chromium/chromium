// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/printers_service_provider.h"

#include "base/logging.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/cups_printers_manager_proxy.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

PrintersServiceProvider::PrintersServiceProvider() = default;

PrintersServiceProvider::~PrintersServiceProvider() = default;

void PrintersServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object_ = exported_object;
  auto* proxy = CupsPrintersManagerFactory::GetInstance()->GetProxy();
  DCHECK(proxy);
  printers_manager_observation_.Observe(proxy);
}

void PrintersServiceProvider::OnPrintersChanged(
    chromeos::PrinterClass printer_class,
    const std::vector<chromeos::Printer>& /*printers*/) {
  // Signal is suppressed for discovered printers because they require setup
  // before being usable.
  if (printer_class == chromeos::PrinterClass::kDiscovered) {
    return;
  }
  DVLOG(1) << "Emitting printers changed DBus event";
  EmitSignal();
}

void PrintersServiceProvider::EmitSignal() {
  DCHECK(exported_object_);

  dbus::Signal signal(chromeos::kPrintersServiceInterface,
                      chromeos::kPrintersServicePrintersChangedSignal);
  exported_object_->SendSignal(&signal);
}

}  // namespace ash

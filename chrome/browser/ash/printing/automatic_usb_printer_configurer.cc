// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/automatic_usb_printer_configurer.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/printing/usb_printer_notification_controller.h"

namespace ash {
namespace {

bool IsPrinterIdInList(const std::string& printer_id,
                       const std::vector<chromeos::Printer>& printer_list) {
  for (const auto& printer : printer_list) {
    if (printer.id() == printer_id) {
      return true;
    }
  }
  return false;
}

}  // namespace

AutomaticUsbPrinterConfigurer::AutomaticUsbPrinterConfigurer(
    std::unique_ptr<PrinterConfigurer> printer_configurer,
    PrinterInstallationManager* installation_manager,
    UsbPrinterNotificationController* notification_controller)
    : printer_configurer_(std::move(printer_configurer)),
      installation_manager_(installation_manager),
      notification_controller_(notification_controller) {
  DCHECK(installation_manager);
}

AutomaticUsbPrinterConfigurer::~AutomaticUsbPrinterConfigurer() = default;

void AutomaticUsbPrinterConfigurer::OnPrintersChanged(
    chromeos::PrinterClass printer_class,
    const std::vector<chromeos::Printer>& printers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

  if (printer_class == chromeos::PrinterClass::kAutomatic) {
    // Remove any notifications for printers that are no longer in the automatic
    // class and setup any USB printers we haven't seen yet.
    PruneRemovedAutomaticPrinters(printers);
    for (const chromeos::Printer& printer : printers) {
      if (!configured_printers_.contains(printer.id()) &&
          printer.IsUsbProtocol()) {
        SetupPrinter(printer);
      }
    }
    return;
  }

  if (printer_class == chromeos::PrinterClass::kDiscovered) {
    // Remove any notifications for printers that are no longer in the
    // discovered class and show a configuration notification for printers we
    // haven't seen yet
    PruneRemovedDiscoveredPrinters(printers);
    for (const chromeos::Printer& printer : printers) {
      if (!unconfigured_printers_.contains(printer.id()) &&
          printer.IsUsbProtocol()) {
        notification_controller_->ShowConfigurationNotification(printer);
        DCHECK(!configured_printers_.contains(printer.id()));
        unconfigured_printers_.insert(printer.id());
      }
    }
    return;
  }
}

void AutomaticUsbPrinterConfigurer::SetupPrinter(
    const chromeos::Printer& printer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  if (installation_manager_->IsPrinterInstalled(printer)) {
    // Skip setup if the printer is already installed.
    CompleteConfiguration(printer);
  }

  printer_configurer_->SetUpPrinter(
      printer, base::BindOnce(&AutomaticUsbPrinterConfigurer::OnSetupComplete,
                              weak_factory_.GetWeakPtr(), printer));
}

void AutomaticUsbPrinterConfigurer::OnSetupComplete(
    const chromeos::Printer& printer,
    PrinterSetupResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  if (result == PrinterSetupResult::kPrinterIsNotAutoconfigurable) {
    installation_manager_->PrinterIsNotAutoconfigurable(printer);
    return;
  }
  if (result != PrinterSetupResult::kSuccess) {
    LOG(ERROR) << "Unable to autoconfigure usb printer " << printer.id();
    return;
  }
  installation_manager_->PrinterInstalled(printer, /*is_automatic=*/true);
  PrinterConfigurer::RecordUsbPrinterSetupSource(
      UsbPrinterSetupSource::kAutoconfigured);
  CompleteConfiguration(printer);
}

void AutomaticUsbPrinterConfigurer::CompleteConfiguration(
    const chromeos::Printer& printer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  VLOG(1) << "Auto USB Printer setup successful for " << printer.id();

  notification_controller_->ShowEphemeralNotification(printer);
  DCHECK(!unconfigured_printers_.contains(printer.id()));
  configured_printers_.insert(printer.id());
}

void AutomaticUsbPrinterConfigurer::PruneRemovedAutomaticPrinters(
    const std::vector<chromeos::Printer>& automatic_printers) {
  PruneRemovedPrinters(automatic_printers, /*use_configured_printers=*/true);
}

void AutomaticUsbPrinterConfigurer::PruneRemovedDiscoveredPrinters(
    const std::vector<chromeos::Printer>& discovered_printers) {
  PruneRemovedPrinters(discovered_printers, /*use_configured_printers=*/false);
}

void AutomaticUsbPrinterConfigurer::PruneRemovedPrinters(
    const std::vector<chromeos::Printer>& current_printers,
    bool use_configured_printers) {
  auto& printers =
      use_configured_printers ? configured_printers_ : unconfigured_printers_;
  for (auto it = printers.begin(); it != printers.end();) {
    if (!IsPrinterIdInList(*it, current_printers)) {
      notification_controller_->RemoveNotification(*it);
      it = printers.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace ash

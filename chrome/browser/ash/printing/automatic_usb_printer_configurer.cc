// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/automatic_usb_printer_configurer.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/printing/printer_installation_manager.h"
#include "chrome/browser/ash/printing/usb_printer_notification_controller.h"
#include "chromeos/printing/ppd_provider.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

AutomaticUsbPrinterConfigurer::AutomaticUsbPrinterConfigurer(
    PrinterInstallationManager* installation_manager,
    UsbPrinterNotificationController* notification_controller,
    chromeos::PpdProvider* ppd_provider,
    base::RepeatingCallback<void(std::string)> refresh_callback)
    : installation_manager_(installation_manager),
      notification_controller_(notification_controller),
      ppd_provider_(ppd_provider),
      refresh_callback_(refresh_callback) {
  DCHECK(installation_manager);
  DCHECK(notification_controller);
  DCHECK(ppd_provider);
}

AutomaticUsbPrinterConfigurer::~AutomaticUsbPrinterConfigurer() = default;

void AutomaticUsbPrinterConfigurer::UpdateListOfConnectedPrinters(
    std::vector<PrinterDetector::DetectedPrinter> new_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

  // Calculate lists of added, existing and removed printers.
  base::flat_set<std::string> added;
  base::flat_set<std::string> existing;
  base::flat_set<std::string> removed;
  for (PrinterDetector::DetectedPrinter& detected : new_list) {
    const std::string& id = detected.printer.id();
    if (base::Contains(connected_printers_, id)) {
      existing.insert(id);
    } else {
      added.insert(id);
      connected_printers_[id] = std::move(detected);
    }
  }
  for (const auto& [id, detected] : connected_printers_) {
    if (!base::Contains(added, id) && !base::Contains(existing, id)) {
      removed.insert(id);
    }
  }

  // Process removed printers.
  for (const std::string& id : removed) {
    notification_controller_->RemoveNotification(id);
    installation_manager_->UninstallPrinter(id);
    connected_printers_.erase(id);
    configured_printers_.erase(id);
    unconfigured_printers_.erase(id);
    ppd_references_.erase(id);
  }

  // Process added printers.
  for (const std::string& id : added) {
    ConfigurePrinter(id);
  }
}

void AutomaticUsbPrinterConfigurer::ConfigurePrinter(
    const std::string& printer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

  PrinterDetector::DetectedPrinter& detected = connected_printers_[printer_id];

  if (detected.printer.RequiresDriverlessUsb()) {
    // This model should attempt autoconfiguration with IPP-USB instead of
    // looking up a PPD for the USB printer class.
    // We copy the printer's object to adjust its configuration before setup.
    // The new printer's configuration is saved to `connected_printers_` only if
    // the setup succeeds.
    chromeos::Printer printer = detected.printer;
    printer.SetUri(chromeos::Uri(base::StringPrintf(
        "ippusb://%04x_%04x/ipp/print", detected.ppd_search_data.usb_vendor_id,
        detected.ppd_search_data.usb_product_id)));
    printer.mutable_ppd_reference()->autoconf = true;
    installation_manager_->SetUpPrinter(
        printer, /*is_automatic_installation=*/true,
        base::BindOnce(&AutomaticUsbPrinterConfigurer::OnSetupComplete,
                       weak_factory_.GetWeakPtr(), printer));
    return;
  }

  // We can start PPD resolution only if there is no other pending resolution
  // for the same `printer_id`. This may happen when a user connects and
  // disconnects USB printer several times in a short period of time.
  if (pending_ppd_resolutions_.insert(printer_id).second) {
    // Insertion took place.
    ppd_provider_->ResolvePpdReference(
        detected.ppd_search_data,
        base::BindOnce(
            &AutomaticUsbPrinterConfigurer::OnResolvePpdReferenceDone,
            weak_factory_.GetWeakPtr(), printer_id));
  }
}

// Callback invoked on completion of PpdProvider::ResolvePpdReference.
void AutomaticUsbPrinterConfigurer::OnResolvePpdReferenceDone(
    const std::string& printer_id,
    chromeos::PpdProvider::CallbackResultCode code,
    const chromeos::Printer::PpdReference& ref,
    const std::string& usb_manufacturer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

  pending_ppd_resolutions_.erase(printer_id);
  auto it = connected_printers_.find(printer_id);
  if (it == connected_printers_.end()) {
    return;
  }

  PrinterDetector::DetectedPrinter& detected = it->second;

  if (code != chromeos::PpdProvider::SUCCESS) {
    PRINTER_LOG(EVENT) << printer_id << ": Failed to resolve PPD reference: "
                       << chromeos::PpdProvider::CallbackResultCodeName(code);
    if (!detected.printer.supports_ippusb()) {
      // Detected printer does not supports ipp-over-usb, so we cannot set it
      // up automatically. We have to leave it as unconfigured.
      if (!usb_manufacturer.empty()) {
        detected.printer.set_usb_printer_manufacturer(usb_manufacturer);
      }
      FinalizeConfiguration(detected.printer, /*success=*/false);
      return;
    }
  }

  // We copy the printer's object to adjust its configuration before setup. The
  // new printer's configuration is saved to `connected_printers_` only if the
  // setup succeeds.
  chromeos::Printer printer = detected.printer;
  // If the printer supports ipp-over-usb and has a PPD, it will be affected by
  // the PPD -> IPP-over-USB migration. We need to mark it to gather extra info
  // in dedicated histograms.
  if (printer.supports_ippusb() && code == chromeos::PpdProvider::SUCCESS) {
    printer.SetAffectedByIppUsbMigration(true);
  }

  // Experimental path (b/184293121).
  const bool force_ipp =
      detected.printer.supports_ippusb() &&
      base::FeatureList::IsEnabled(features::kIppFirstSetupForUsbPrinters);

  if (code == chromeos::PpdProvider::SUCCESS && !force_ipp) {
    *printer.mutable_ppd_reference() = ref;
  } else {
    // Detected printer supports ipp-over-usb. We can try to set it up
    // automatically (by IPP Everywhere). We have to switch to the ippusb
    // scheme.
    printer.SetUri(chromeos::Uri(base::StringPrintf(
        "ippusb://%04x_%04x/ipp/print", detected.ppd_search_data.usb_vendor_id,
        detected.ppd_search_data.usb_product_id)));
    printer.mutable_ppd_reference()->autoconf = true;
    // If we have PpdReference, we save it for later.
    if (code == chromeos::PpdProvider::SUCCESS) {
      ppd_references_[printer_id] = ref;
    }
  }

  installation_manager_->SetUpPrinter(
      printer, /*is_automatic_installation=*/true,
      base::BindOnce(&AutomaticUsbPrinterConfigurer::OnSetupComplete,
                     weak_factory_.GetWeakPtr(), printer));
}

void AutomaticUsbPrinterConfigurer::OnSetupComplete(
    const chromeos::Printer& printer,
    PrinterSetupResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

  auto it = connected_printers_.find(printer.id());
  if (it == connected_printers_.end()) {
    return;
  }

  if (printer.AffectedByIppUsbMigration()) {
    base::UmaHistogramEnumeration(
        "Printing.CUPS.AutomaticSetupResultOfUsbPrinterSupportingIppAndPpd",
        result);
  }

  auto it_ref = ppd_references_.find(printer.id());
  if (it_ref != ppd_references_.end()) {
    // We have a PPD for this printer. We can try to use it if IPP Everywhere
    // setup failed.
    if (result == PrinterSetupResult::kPrinterIsNotAutoconfigurable) {
      // Repeat the setup procedure with the given PPD file.
      chromeos::Printer ppd_printer = it->second.printer;
      *ppd_printer.mutable_ppd_reference() = std::move(it_ref->second);
      ppd_references_.erase(it_ref);
      installation_manager_->SetUpPrinter(
          ppd_printer, /*is_automatic_installation=*/true,
          base::BindOnce(&AutomaticUsbPrinterConfigurer::OnSetupComplete,
                         weak_factory_.GetWeakPtr(), ppd_printer));
      return;
    }
    // Nevermind. Just remove it.
    ppd_references_.erase(it_ref);
  }

  const bool success = (result == PrinterSetupResult::kSuccess);
  if (success) {
    it->second.printer = printer;
  }
  FinalizeConfiguration(it->second.printer, success);
}

void AutomaticUsbPrinterConfigurer::FinalizeConfiguration(
    const chromeos::Printer& printer,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  DCHECK(!configured_printers_.contains(printer.id()));
  DCHECK(!unconfigured_printers_.contains(printer.id()));

  if (success) {
    // The printer is ready to use
    PrinterConfigurer::RecordUsbPrinterSetupSource(
        UsbPrinterSetupSource::kAutoconfigured);
    PRINTER_LOG(EVENT) << printer.id()
                       << ": Automatic USB printer setup succeeded";
    notification_controller_->ShowEphemeralNotification(printer);
    configured_printers_.insert(printer.id());
  } else {
    // The printer cannot be configured automatically
    PRINTER_LOG(EVENT) << printer.id()
                       << ": Unable to autoconfigure USB printer";
    notification_controller_->ShowConfigurationNotification(printer);
    unconfigured_printers_.insert(printer.id());
  }
  refresh_callback_.Run(printer.id());
}

}  // namespace ash

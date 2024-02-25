// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"

#include <string>
#include <utility>

#include "base/observer_list.h"
#include "chrome/browser/ash/printing/printer_configurer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ::chromeos::CupsPrinterStatus;
using ::chromeos::Printer;
using ::chromeos::PrinterClass;

FakeCupsPrintersManager::FakeCupsPrintersManager() = default;

FakeCupsPrintersManager::~FakeCupsPrintersManager() = default;

std::vector<Printer> FakeCupsPrintersManager::GetPrinters(
    PrinterClass printer_class) const {
  return printers_.Get(printer_class);
}

void FakeCupsPrintersManager::SavePrinter(const chromeos::Printer& printer) {
  printers_.Insert(PrinterClass::kSaved, printer);
}

void FakeCupsPrintersManager::RemoveSavedPrinter(
    const std::string& printer_id) {
  installed_.erase(printer_id);
  printers_.Remove(PrinterClass::kSaved, printer_id);
}

void FakeCupsPrintersManager::AddLocalPrintersObserver(
    LocalPrintersObserver* observer) {
  local_printers_observer_list_.AddObserver(observer);
}

void FakeCupsPrintersManager::RemoveLocalPrintersObserver(
    LocalPrintersObserver* observer) {
  local_printers_observer_list_.RemoveObserver(observer);
}

bool FakeCupsPrintersManager::IsPrinterInstalled(
    const chromeos::Printer& printer) const {
  return installed_.contains(printer.id());
}

void FakeCupsPrintersManager::SetUpPrinter(const chromeos::Printer& printer,
                                           bool is_automatic_installation,
                                           PrinterSetupCallback callback) {
  auto it = assigned_results_.find(printer.id());
  PrinterSetupResult result =
      it != assigned_results_.end() ? it->second : PrinterSetupResult::kSuccess;
  if (result == PrinterSetupResult::kSuccess) {
    installed_.insert(printer.id());
  }
  std::move(callback).Run(result);
}

void FakeCupsPrintersManager::UninstallPrinter(const std::string& printer_id) {
  installed_.erase(printer_id);
}

std::optional<Printer> FakeCupsPrintersManager::GetPrinter(
    const std::string& id) const {
  return printers_.Get(id);
}

void FakeCupsPrintersManager::FetchPrinterStatus(const std::string& printer_id,
                                                 PrinterStatusCallback cb) {
  auto it = printer_status_map_.find(printer_id);
  if (it == printer_status_map_.end()) {
    FAIL() << "Printer status not found: " << printer_id;
  }
  std::move(cb).Run(std::move(it->second));
  printer_status_map_.erase(it);
}

PrintServersManager* FakeCupsPrintersManager::GetPrintServersManager() const {
  return nullptr;
}

void FakeCupsPrintersManager::AddPrinter(const chromeos::Printer& printer,
                                         PrinterClass printer_class) {
  printers_.Insert(printer_class, printer);
}

void FakeCupsPrintersManager::SetPrinterStatus(
    const CupsPrinterStatus& status) {
  printer_status_map_[status.GetPrinterId()] = status;
}

void FakeCupsPrintersManager::MarkInstalled(const std::string& printer_id) {
  installed_.insert(printer_id);
}

void FakeCupsPrintersManager::MarkPrinterAsNotAutoconfigurable(
    const std::string& printer_id) {
  printers_marked_as_not_autoconf_.insert(printer_id);
}

void FakeCupsPrintersManager::SetPrinterSetupResult(
    const std::string& printer_id,
    PrinterSetupResult result) {
  assigned_results_[printer_id] = result;
}

void FakeCupsPrintersManager::QueryPrinterForAutoConf(
    const Printer& printer,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(
      !printers_marked_as_not_autoconf_.contains(printer.id()));
}

void FakeCupsPrintersManager::TriggerLocalPrintersObserver() {
  for (auto& observer : local_printers_observer_list_) {
    observer.OnLocalPrintersUpdated();
  }
}

}  // namespace ash

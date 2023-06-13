// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/test_cups_printers_manager.h"

#include <string>
#include <utility>

#include "chrome/browser/ash/printing/printer_configurer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ::chromeos::CupsPrinterStatus;
using ::chromeos::Printer;
using ::chromeos::PrinterClass;

TestCupsPrintersManager::TestCupsPrintersManager() = default;

TestCupsPrintersManager::~TestCupsPrintersManager() = default;

std::vector<Printer> TestCupsPrintersManager::GetPrinters(
    PrinterClass printer_class) const {
  return printers_.Get(printer_class);
}

bool TestCupsPrintersManager::IsPrinterInstalled(const Printer& printer) const {
  return installed_.contains(printer.id());
}

absl::optional<Printer> TestCupsPrintersManager::GetPrinter(
    const std::string& id) const {
  return printers_.Get(id);
}

void TestCupsPrintersManager::FetchPrinterStatus(const std::string& printer_id,
                                                 PrinterStatusCallback cb) {
  auto it = printer_status_map_.find(printer_id);
  if (it == printer_status_map_.end()) {
    FAIL() << "Printer status not found: " << printer_id;
  }
  std::move(cb).Run(std::move(it->second));
  printer_status_map_.erase(it);
}

// Add |printer| to the corresponding list in |printers_| bases on the given
// |printer_class|.
void TestCupsPrintersManager::AddPrinter(const Printer& printer,
                                         PrinterClass printer_class) {
  printers_.Insert(printer_class, printer);
}

void TestCupsPrintersManager::InstallPrinter(const std::string& id) {
  EXPECT_TRUE(installed_.insert(id).second);
}

void TestCupsPrintersManager::SetPrinterStatus(
    const CupsPrinterStatus& status) {
  printer_status_map_[status.GetPrinterId()] = status;
}

void TestCupsPrintersManager::SetUpPrinter(const chromeos::Printer& printer,
                                           PrinterSetupCallback callback) {
  MarkConfigured(printer.id());
  auto it = assigned_results_.find(printer.id());
  PrinterSetupResult result =
      it != assigned_results_.end() ? it->second : PrinterSetupResult::kSuccess;
  std::move(callback).Run(result);
}

void TestCupsPrintersManager::UninstallPrinter(const std::string& printer_id) {
  configured_printers_.erase(printer_id);
}

bool TestCupsPrintersManager::IsConfigured(
    const std::string& printer_id) const {
  return configured_printers_.contains(printer_id);
}

void TestCupsPrintersManager::MarkConfigured(const std::string& printer_id) {
  configured_printers_.insert(printer_id);
  InstallPrinter(printer_id);
}

void TestCupsPrintersManager::SetPrinterSetupResult(
    const std::string& printer_id,
    PrinterSetupResult result) {
  assigned_results_[printer_id] = result;
}

}  // namespace ash

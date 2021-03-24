// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/test_printer_configurer.h"

#include "base/callback.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

TestPrinterConfigurer::TestPrinterConfigurer() = default;

TestPrinterConfigurer::~TestPrinterConfigurer() = default;

void TestPrinterConfigurer::SetUpPrinter(const Printer& printer,
                                         PrinterSetupCallback callback) {
  MarkConfigured(printer.id());
  auto it = assigned_results_.find(printer.id());
  PrinterSetupResult result =
      it != assigned_results_.end() ? it->second : PrinterSetupResult::kSuccess;
  std::move(callback).Run(result);
}

bool TestPrinterConfigurer::IsConfigured(const std::string& printer_id) const {
  return configured_printers_.contains(printer_id);
}

void TestPrinterConfigurer::MarkConfigured(const std::string& printer_id) {
  configured_printers_.insert(printer_id);
}

void TestPrinterConfigurer::AssignPrinterSetupResult(
    const std::string& printer_id,
    PrinterSetupResult result) {
  assigned_results_[printer_id] = result;
}

}  // namespace chromeos

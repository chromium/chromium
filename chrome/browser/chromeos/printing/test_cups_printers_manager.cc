// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/test_cups_printers_manager.h"

namespace chromeos {

TestCupsPrintersManager::TestCupsPrintersManager() = default;

TestCupsPrintersManager::~TestCupsPrintersManager() = default;

std::vector<Printer> TestCupsPrintersManager::GetPrinters(
    PrinterClass printer_class) const {
  return printers_.Get(printer_class);
}

bool TestCupsPrintersManager::IsPrinterInstalled(const Printer& printer) const {
  return installed_.contains(printer.id());
}

base::Optional<Printer> TestCupsPrintersManager::GetPrinter(
    const std::string& id) const {
  return printers_.Get(id);
}

// Add |printer| to the corresponding list in |printers_| bases on the given
// |printer_class|.
void TestCupsPrintersManager::AddPrinter(const Printer& printer,
                                         PrinterClass printer_class) {
  printers_.Insert(printer_class, printer);
}

void TestCupsPrintersManager::InstallPrinter(const std::string& id) {
  installed_.insert(id);
}

}  // namespace chromeos

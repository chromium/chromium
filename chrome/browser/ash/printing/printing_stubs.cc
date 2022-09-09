// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printing_stubs.h"

namespace ash {

std::vector<chromeos::Printer> StubCupsPrintersManager::GetPrinters(
    chromeos::PrinterClass printer_class) const {
  return {};
}

bool StubCupsPrintersManager::IsPrinterInstalled(
    const chromeos::Printer& printer) const {
  return false;
}

absl::optional<chromeos::Printer> StubCupsPrintersManager::GetPrinter(
    const std::string& id) const {
  return {};
}

PrintServersManager* StubCupsPrintersManager::GetPrintServersManager() const {
  return nullptr;
}

}  // namespace ash

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printing_stubs.h"

namespace chromeos {

std::vector<Printer> StubCupsPrintersManager::GetPrinters(
    PrinterClass printer_class) const {
  return {};
}

bool StubCupsPrintersManager::IsPrinterInstalled(const Printer& printer) const {
  return false;
}

base::Optional<Printer> StubCupsPrintersManager::GetPrinter(
    const std::string& id) const {
  return {};
}

}  // namespace chromeos

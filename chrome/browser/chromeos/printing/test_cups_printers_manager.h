// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_TEST_CUPS_PRINTERS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_TEST_CUPS_PRINTERS_MANAGER_H_

#include "base/containers/flat_set.h"
#include "chrome/browser/chromeos/printing/printers_map.h"
#include "chrome/browser/chromeos/printing/printing_stubs.h"

namespace chromeos {

// Test printers manager which allows to add the printer of arbitrary class.
// It's used in unit and API integration tests.
class TestCupsPrintersManager : public StubCupsPrintersManager {
 public:
  TestCupsPrintersManager();
  ~TestCupsPrintersManager() override;

  // CupsPrintersManager:
  std::vector<Printer> GetPrinters(PrinterClass printer_class) const override;
  bool IsPrinterInstalled(const Printer& printer) const override;
  base::Optional<Printer> GetPrinter(const std::string& id) const override;

  void AddPrinter(const Printer& printer, PrinterClass printer_class);
  void InstallPrinter(const std::string& id);

 private:
  PrintersMap printers_;
  base::flat_set<std::string> installed_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_TEST_CUPS_PRINTERS_MANAGER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_TEST_CUPS_PRINTERS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_TEST_CUPS_PRINTERS_MANAGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/chromeos/printing/printers_map.h"
#include "chrome/browser/chromeos/printing/printing_stubs.h"
#include "chromeos/printing/cups_printer_status.h"

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
  absl::optional<Printer> GetPrinter(const std::string& id) const override;
  void FetchPrinterStatus(const std::string& printer_id,
                          PrinterStatusCallback cb) override;

  void AddPrinter(const Printer& printer, PrinterClass printer_class);
  void InstallPrinter(const std::string& id);
  void SetPrinterStatus(const chromeos::CupsPrinterStatus& status);

 private:
  // Map printer id to CupsPrinterStatus object.
  base::flat_map<std::string, chromeos::CupsPrinterStatus> printer_status_map_;
  PrintersMap printers_;
  base::flat_set<std::string> installed_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_TEST_CUPS_PRINTERS_MANAGER_H_

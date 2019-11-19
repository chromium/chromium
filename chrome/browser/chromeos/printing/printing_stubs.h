// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTING_STUBS_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTING_STUBS_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

class StubCupsPrintersManager : public CupsPrintersManager {
 public:
  StubCupsPrintersManager() = default;

  std::vector<Printer> GetPrinters(PrinterClass printer_class) const override;
  bool IsPrinterInstalled(const Printer& printer) const override;
  base::Optional<Printer> GetPrinter(const std::string& id) const override;

  void SavePrinter(const Printer& printer) override {}
  void RemoveSavedPrinter(const std::string& printer_id) override {}
  void AddObserver(CupsPrintersManager::Observer* observer) override {}
  void RemoveObserver(CupsPrintersManager::Observer* observer) override {}
  void PrinterInstalled(const Printer& printer,
                        bool is_automatic,
                        PrinterSetupSource source) override {}
  void RecordSetupAbandoned(const Printer& printer) override {}
};

class StubPrinterConfigurer : public PrinterConfigurer {
 public:
  void SetUpPrinter(const Printer& printer,
                    PrinterSetupCallback callback) override {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTING_STUBS_H_

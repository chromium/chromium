// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINTING_STUBS_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINTING_STUBS_H_

#include <string>
#include <vector>

#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/printer_configurer.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class StubCupsPrintersManager : public CupsPrintersManager {
 public:
  StubCupsPrintersManager() = default;

  std::vector<chromeos::Printer> GetPrinters(
      chromeos::PrinterClass printer_class) const override;
  bool IsPrinterInstalled(const chromeos::Printer& printer) const override;
  absl::optional<chromeos::Printer> GetPrinter(
      const std::string& id) const override;
  PrintServersManager* GetPrintServersManager() const override;

  void SavePrinter(const chromeos::Printer& printer) override {}
  void RemoveSavedPrinter(const std::string& printer_id) override {}
  void AddObserver(CupsPrintersManager::Observer* observer) override {}
  void RemoveObserver(CupsPrintersManager::Observer* observer) override {}
  void PrinterInstalled(const chromeos::Printer& printer,
                        bool is_automatic) override {}
  void PrinterIsNotAutoconfigurable(const chromeos::Printer& printer) override {
  }
  void RecordSetupAbandoned(const chromeos::Printer& printer) override {}
  void FetchPrinterStatus(const std::string& printer_id,
                          PrinterStatusCallback cb) override {}
  void RecordNearbyNetworkPrinterCounts() const override {}
};

class StubPrinterConfigurer : public PrinterConfigurer {
 public:
  void SetUpPrinter(const chromeos::Printer& printer,
                    PrinterSetupCallback callback) override {}
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINTING_STUBS_H_

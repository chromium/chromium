// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_FAKE_CUPS_PRINTERS_MANAGER_H_
#define CHROME_BROWSER_ASH_PRINTING_FAKE_CUPS_PRINTERS_MANAGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/printer_configurer.h"
#include "chrome/browser/ash/printing/printers_map.h"
#include "chromeos/printing/cups_printer_status.h"

namespace ash {

// Fake printers manager which allows to add the printer of arbitrary class.
// It's used in unit and API integration tests.
class FakeCupsPrintersManager : public CupsPrintersManager {
 public:
  FakeCupsPrintersManager();
  ~FakeCupsPrintersManager() override;

  std::vector<chromeos::Printer> GetPrinters(
      chromeos::PrinterClass printer_class) const override;
  void SavePrinter(const chromeos::Printer& printer) override;
  void RemoveSavedPrinter(const std::string& printer_id) override;

  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}

  void PrinterInstalled(const chromeos::Printer& printer,
                        bool is_automatic) override;
  bool IsPrinterInstalled(const chromeos::Printer& printer) const override;
  void PrinterIsNotAutoconfigurable(const chromeos::Printer& printer) override;
  void SetUpPrinter(const chromeos::Printer& printer,
                    PrinterSetupCallback callback) override;
  void UninstallPrinter(const std::string& printer_id) override;
  absl::optional<chromeos::Printer> GetPrinter(
      const std::string& id) const override;

  void RecordSetupAbandoned(const chromeos::Printer& printer) override {}

  void FetchPrinterStatus(const std::string& printer_id,
                          PrinterStatusCallback cb) override;

  void RecordNearbyNetworkPrinterCounts() const override {}
  PrintServersManager* GetPrintServersManager() const override;

  // Add |printer| to the corresponding list in |printers_| based on the given
  // |printer_class|.
  void AddPrinter(const chromeos::Printer& printer,
                  chromeos::PrinterClass printer_class);
  void SetPrinterStatus(const chromeos::CupsPrinterStatus& status);
  // Returns true if the printer with given |printer_id| was set up or
  // explicitly marked as configured before.
  bool IsConfigured(const std::string& printer_id) const;
  void MarkConfigured(const std::string& printer_id);
  void SetPrinterSetupResult(const std::string& printer_id,
                             PrinterSetupResult result);
  bool IsMarkedAsNotAutoconfigurable(const chromeos::Printer& printer);

 private:
  // Map printer id to CupsPrinterStatus object.
  base::flat_map<std::string, chromeos::CupsPrinterStatus> printer_status_map_;
  PrintersMap printers_;
  base::flat_set<std::string> installed_;

  base::flat_set<std::string> configured_printers_;
  base::flat_set<std::string> printers_marked_as_not_autoconf_;
  base::flat_map<std::string, PrinterSetupResult> assigned_results_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_FAKE_CUPS_PRINTERS_MANAGER_H_

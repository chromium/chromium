// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_TEST_PRINTER_CONFIGURER_H_
#define CHROME_BROWSER_ASH_PRINTING_TEST_PRINTER_CONFIGURER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/ash/printing/printer_configurer.h"

namespace chromeos {
class Printer;
}

namespace ash {

class TestCupsPrintersManager;

// Test PrinterConfigurer which allows printers to be marked as configured for
// unit tests.
// Does not configure printers. Can be used to verify that configuration was
// initiated by the class under test.
class TestPrinterConfigurer : public PrinterConfigurer {
 public:
  TestPrinterConfigurer();
  explicit TestPrinterConfigurer(TestCupsPrintersManager* manager);

  ~TestPrinterConfigurer() override;

  // PrinterConfigurer:
  void SetUpPrinter(const chromeos::Printer& printer,
                    PrinterSetupCallback callback) override;

  // Returns true if the printer with given |printer_id| was set up or
  // explicitly marked as configured before.
  bool IsConfigured(const std::string& printer_id) const;

  void MarkConfigured(const std::string& printer_id);

  void AssignPrinterSetupResult(const std::string& printer_id,
                                PrinterSetupResult result);

 private:
  TestCupsPrintersManager* manager_ = nullptr;
  base::flat_set<std::string> configured_printers_;
  base::flat_map<std::string, PrinterSetupResult> assigned_results_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_TEST_PRINTER_CONFIGURER_H_

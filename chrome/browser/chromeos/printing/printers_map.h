// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_MAP_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_MAP_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

// PrintersMap stores printers, categorized by class.
class PrintersMap {
 public:
  PrintersMap();
  ~PrintersMap();

  // Returns printer matching |printer_id| if found in any PrinterClass.
  base::Optional<Printer> Get(const std::string& printer_id) const;

  // Returns printer matching |printer_id| in |printer_class|.
  base::Optional<Printer> Get(PrinterClass printer_class,
                              const std::string& printer_id) const;

  // Returns all printers across all classes.
  std::vector<Printer> Get() const;

  // Returns all printers in |printer_class|.
  std::vector<Printer> Get(PrinterClass printer_class) const;

  // Returns all printers in |printer_class| that have a secure protocol
  // (non-network or IPPS).
  std::vector<Printer> GetSecurePrinters(PrinterClass printer_class) const;

  // Returns all printers that have a secure protocol (non-network or IPPS).
  std::vector<Printer> GetSecurePrinters() const;

  // Adds |printer| to |printer_class|.
  void Insert(PrinterClass printer_class, const Printer& printer);

  // Removes all printers in |printer_class|.
  void Clear(PrinterClass printer_class);

  // Replaces the printers in |printer_class| with |printers|.
  void ReplacePrintersInClass(PrinterClass printer_class,
                              const std::vector<Printer>& printers);

  // Removes printer with |printer_id| from |printer_class|. This is a no-op if
  // |printer_id| doesn't exist in printer_class.
  void Remove(PrinterClass printer_class, const std::string& printer_id);

  // Returns true if the printer |printer_id| exists in |printer_class|.
  bool IsPrinterInClass(PrinterClass printer_class,
                        const std::string& printer_id) const;

 private:
  // Returns true if |printer_class| exists and contains at least 1 printer.
  bool HasPrintersInClass(PrinterClass printer_class) const;

  // Returns true if |printer_id| exists in any class. Used only for DCHECKs.
  bool IsExistingPrinter(const std::string& printer_id) const;

  // Categorized printers. Outer map keyed on PrinterClass, inner map keyed on
  // PrinterId.
  std::unordered_map<PrinterClass, std::unordered_map<std::string, Printer>>
      printers_;

  DISALLOW_COPY_AND_ASSIGN(PrintersMap);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_MAP_H_

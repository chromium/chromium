// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_MAP_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_MAP_H_

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/flat_map.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/printer_configuration.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

// PrintersMap stores printers, categorized by class.
class PrintersMap {
 public:
  PrintersMap();

  PrintersMap(const PrintersMap&) = delete;
  PrintersMap& operator=(const PrintersMap&) = delete;

  ~PrintersMap();

  // Returns printer matching |printer_id| if found in any PrinterClass.
  absl::optional<Printer> Get(const std::string& printer_id) const;

  // Returns printer matching |printer_id| in |printer_class|.
  absl::optional<Printer> Get(PrinterClass printer_class,
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

  // Adds |printer| with status |cups_printer_status| to |printer_class|.
  void Insert(PrinterClass printer_class,
              const Printer& printer,
              const CupsPrinterStatus& cups_printer_status);

  // Removes all printers in |printer_class|.
  void Clear(PrinterClass printer_class);

  // Replaces the printers in |printer_class| with |printers|. Adds a status to
  // the printer if a status was previously saved in the printer status map.
  void ReplacePrintersInClass(PrinterClass printer_class,
                              const std::vector<Printer>& printers);

  // Removes printer with |printer_id| from |printer_class|. This is a no-op if
  // |printer_id| doesn't exist in printer_class.
  void Remove(PrinterClass printer_class, const std::string& printer_id);

  // Returns true if the printer |printer_id| exists in |printer_class|.
  bool IsPrinterInClass(PrinterClass printer_class,
                        const std::string& printer_id) const;

  // Adds printer status to existing printers in |printers_| map and also saves
  // to |printer_statuses_| cache for future printer retrievals.
  void SavePrinterStatus(const std::string& printer_id,
                         const CupsPrinterStatus& cups_printer_status);

 private:
  // Returns true if |printer_class| exists and contains at least 1 printer.
  bool HasPrintersInClass(PrinterClass printer_class) const;

  // Returns true if |printer_id| exists in any class. Used only for DCHECKs.
  bool IsExistingPrinter(const std::string& printer_id) const;

  absl::optional<CupsPrinterStatus> GetPrinterStatus(
      const std::string& printer_id) const;

  // Returns set of printer id's for printers in class |printer_class|.
  std::set<std::string> GetPrinterIdsInClass(PrinterClass printer_class) const;

  // Categorized printers. Outer map keyed on PrinterClass, inner map keyed on
  // PrinterId.
  std::unordered_map<PrinterClass, std::unordered_map<std::string, Printer>>
      printers_;

  // Stores printer statuses returned from performing printer status queries.
  // This map is used to persist the printer statuses so when |printers_| map is
  // rebuilt, all the statuses aren't lost. Key for this map is a printer id.
  base::flat_map<std::string, CupsPrinterStatus> printer_statuses_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_MAP_H_

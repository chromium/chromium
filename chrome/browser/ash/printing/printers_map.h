// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINTERS_MAP_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINTERS_MAP_H_

#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/flat_map.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/printer_configuration.h"

namespace ash {

// PrintersMap stores printers, categorized by class.
class PrintersMap {
 public:
  PrintersMap();

  PrintersMap(const PrintersMap&) = delete;
  PrintersMap& operator=(const PrintersMap&) = delete;

  ~PrintersMap();

  // Returns printer matching |printer_id| if found in any PrinterClass.
  std::optional<chromeos::Printer> Get(const std::string& printer_id) const;

  // Returns printer matching |printer_id| in |printer_class|.
  std::optional<chromeos::Printer> Get(chromeos::PrinterClass printer_class,
                                       const std::string& printer_id) const;

  // Returns all printers across all classes.
  std::vector<chromeos::Printer> Get() const;

  // Returns all printers in |printer_class|.
  std::vector<chromeos::Printer> Get(
      chromeos::PrinterClass printer_class) const;

  // Returns all printers in |printer_class| that have a secure protocol
  // (non-network or IPPS).
  std::vector<chromeos::Printer> GetSecurePrinters(
      chromeos::PrinterClass printer_class) const;

  // Returns all printers that have a secure protocol (non-network or IPPS).
  std::vector<chromeos::Printer> GetSecurePrinters() const;

  // Adds |printer| to |printer_class|.
  void Insert(chromeos::PrinterClass printer_class,
              const chromeos::Printer& printer);

  // Adds |printer| with status |cups_printer_status| to |printer_class|.
  void Insert(chromeos::PrinterClass printer_class,
              const chromeos::Printer& printer,
              const chromeos::CupsPrinterStatus& cups_printer_status);

  // Removes all printers in |printer_class|.
  void Clear(chromeos::PrinterClass printer_class);

  // Replaces the printers in |printer_class| with |printers|. Adds a status to
  // the printer if a status was previously saved in the printer status map.
  void ReplacePrintersInClass(chromeos::PrinterClass printer_class,
                              const std::vector<chromeos::Printer>& printers);

  // Removes printer with |printer_id| from |printer_class|. This is a no-op if
  // |printer_id| doesn't exist in printer_class.
  void Remove(chromeos::PrinterClass printer_class,
              const std::string& printer_id);

  // Returns true if the printer |printer_id| exists in |printer_class|.
  bool IsPrinterInClass(chromeos::PrinterClass printer_class,
                        const std::string& printer_id) const;

  // Adds printer status to existing printers in |printers_| map and also saves
  // to |printer_statuses_| cache for future printer retrievals. Returns true
  // if this is the printer's first saved status or different than the
  // previously saved status.
  bool SavePrinterStatus(
      const std::string& printer_id,
      const chromeos::CupsPrinterStatus& cups_printer_status);

 private:
  using PrintersInClassMap = std::unordered_map<std::string, chromeos::Printer>;
  using PrinterClassesMap =
      std::unordered_map<chromeos::PrinterClass, PrintersInClassMap>;

  // Returns true if |printer_id| exists in any class. Used only for DCHECKs.
  bool IsExistingPrinter(const std::string& printer_id) const;

  // Returns a const pointer to the map of all printers for a particular
  // |printer_class|, or nullptr if it doesn't exist.
  const PrintersInClassMap* FindPrintersInClassOrNull(
      chromeos::PrinterClass printer_class) const;

  // Returns set of printer id's for printers in class |printer_class|.
  std::set<std::string> GetPrinterIdsInClass(
      chromeos::PrinterClass printer_class) const;

  // Categorized printers. Outer map keyed on PrinterClass, inner map keyed on
  // PrinterId.
  PrinterClassesMap printers_;

  // Stores printer statuses returned from performing printer status queries.
  // This map is used to persist the printer statuses so when |printers_| map is
  // rebuilt, all the statuses aren't lost. Key for this map is a printer id.
  base::flat_map<std::string, chromeos::CupsPrinterStatus> printer_statuses_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINTERS_MAP_H_

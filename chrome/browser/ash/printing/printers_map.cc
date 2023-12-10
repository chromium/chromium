// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printers_map.h"

#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/containers/map_util.h"

namespace ash {

using ::chromeos::CupsPrinterStatus;
using ::chromeos::Printer;
using ::chromeos::PrinterClass;

namespace {

std::vector<Printer> GetPrintersAsVector(
    const std::unordered_map<std::string, Printer>& printers_map,
    bool only_secure) {
  std::vector<Printer> printers;
  for (const auto& [printer_id, printer] : printers_map) {
    if (only_secure && !printer.HasSecureProtocol()) {
      continue;
    }
    printers.push_back(printer);
  }
  return printers;
}

}  // namespace

PrintersMap::PrintersMap() = default;
PrintersMap::~PrintersMap() = default;

std::optional<Printer> PrintersMap::Get(const std::string& printer_id) const {
  for (const auto& [printer_class, printers_map] : printers_) {
    if (auto* printer = base::FindOrNull(printers_map, printer_id)) {
      return *printer;
    }
  }
  return std::nullopt;
}

std::optional<Printer> PrintersMap::Get(PrinterClass printer_class,
                                        const std::string& printer_id) const {
  if (auto* printers_map = FindPrintersInClassOrNull(printer_class)) {
    if (auto* printer = base::FindOrNull(*printers_map, printer_id)) {
      return *printer;
    }
  }
  return std::nullopt;
}

std::vector<Printer> PrintersMap::Get(PrinterClass printer_class) const {
  if (auto* printers_map = FindPrintersInClassOrNull(printer_class)) {
    return GetPrintersAsVector(*printers_map, /*only_secure=*/false);
  }
  return std::vector<Printer>();
}

std::vector<Printer> PrintersMap::Get() const {
  std::vector<Printer> result;
  for (const auto& [printer_class, printers_map] : printers_) {
    base::Extend(result,
                 GetPrintersAsVector(printers_map, /*only_secure=*/false));
  }
  return result;
}

void PrintersMap::Insert(PrinterClass printer_class, const Printer& printer) {
  DCHECK(!IsExistingPrinter(printer.id()));

  printers_[printer_class][printer.id()] = printer;
}

void PrintersMap::Insert(PrinterClass printer_class,
                         const Printer& printer,
                         const CupsPrinterStatus& cups_printer_status) {
  Insert(printer_class, printer);
  printers_[printer_class][printer.id()].set_printer_status(
      cups_printer_status);
}

void PrintersMap::Clear(PrinterClass printer_class) {
  printers_[printer_class].clear();
}

void PrintersMap::ReplacePrintersInClass(PrinterClass printer_class,
                                         const std::vector<Printer>& printers) {
  // Get set of printer ids that initially existed in |printer_class| so the
  // printers that aren't replaced by |printers| have their statuses deleted.
  std::set<std::string> statuses_to_remove =
      GetPrinterIdsInClass(printer_class);

  Clear(printer_class);
  for (const auto& printer : printers) {
    // Printer was replaced so remove the id from the set so its status won't be
    // deleted.
    statuses_to_remove.erase(printer.id());

    if (auto* printer_status =
            base::FindOrNull(printer_statuses_, printer.id())) {
      Insert(printer_class, printer, *printer_status);
      continue;
    }
    Insert(printer_class, printer);
  }

  for (const std::string& printer_id : statuses_to_remove) {
    printer_statuses_.erase(printer_id);
  }
}

std::vector<Printer> PrintersMap::GetSecurePrinters() const {
  std::vector<Printer> result;
  for (const auto& [printer_class, printers_map] : printers_) {
    base::Extend(result,
                 GetPrintersAsVector(printers_map, /*only_secure=*/true));
  }
  return result;
}

std::vector<Printer> PrintersMap::GetSecurePrinters(
    PrinterClass printer_class) const {
  if (auto* printers_map = FindPrintersInClassOrNull(printer_class)) {
    return GetPrintersAsVector(*printers_map, /*only_secure=*/true);
  }
  return std::vector<Printer>();
}

void PrintersMap::Remove(PrinterClass printer_class,
                         const std::string& printer_id) {
  if (!IsPrinterInClass(printer_class, printer_id)) {
    return;
  }
  printers_[printer_class].erase(printer_id);
  printer_statuses_.erase(printer_id);

  DCHECK(!IsExistingPrinter(printer_id));
}

bool PrintersMap::IsPrinterInClass(PrinterClass printer_class,
                                   const std::string& printer_id) const {
  auto* printers_map = FindPrintersInClassOrNull(printer_class);
  return printers_map ? base::Contains(*printers_map, printer_id) : false;
}

bool PrintersMap::IsExistingPrinter(const std::string& printer_id) const {
  return Get(printer_id).has_value();
}

bool PrintersMap::SavePrinterStatus(
    const std::string& printer_id,
    const CupsPrinterStatus& cups_printer_status) {
  printer_statuses_[printer_id] = cups_printer_status;

  for (auto& [printer_class, printers_map] : printers_) {
    if (auto* printer = base::FindOrNull(printers_map, printer_id)) {
      const bool is_new_status =
          printer->printer_status() != cups_printer_status;
      printer->set_printer_status(cups_printer_status);
      return is_new_status;
    }
  }
  return false;
}

std::set<std::string> PrintersMap::GetPrinterIdsInClass(
    PrinterClass printer_class) const {
  std::set<std::string> result;
  if (auto* printers_map = FindPrintersInClassOrNull(printer_class)) {
    for (const auto& [printer_id, printer] : *printers_map) {
      result.insert(printer.id());
    }
  }
  return result;
}

const PrintersMap::PrintersInClassMap* PrintersMap::FindPrintersInClassOrNull(
    chromeos::PrinterClass printer_class) const {
  return base::FindOrNull(printers_, printer_class);
}

}  // namespace ash

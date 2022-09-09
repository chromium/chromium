// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printers_map.h"

#include "base/containers/contains.h"

namespace ash {

using ::chromeos::CupsPrinterStatus;
using ::chromeos::Printer;
using ::chromeos::PrinterClass;

PrintersMap::PrintersMap() = default;
PrintersMap::~PrintersMap() = default;

absl::optional<Printer> PrintersMap::Get(const std::string& printer_id) const {
  for (const auto& kv : printers_) {
    const PrinterClass& printer_class = kv.first;
    const auto& printer_list = kv.second;
    if (IsPrinterInClass(printer_class, printer_id)) {
      return printer_list.at(printer_id);
    }
  }
  return absl::nullopt;
}

absl::optional<Printer> PrintersMap::Get(PrinterClass printer_class,
                                         const std::string& printer_id) const {
  if (!IsPrinterInClass(printer_class, printer_id)) {
    return absl::nullopt;
  }

  return printers_.at(printer_class).at(printer_id);
}

std::vector<Printer> PrintersMap::Get(PrinterClass printer_class) const {
  if (!HasPrintersInClass(printer_class)) {
    return std::vector<Printer>();
  }

  std::vector<Printer> result;
  result.reserve(printers_.at(printer_class).size());
  for (const auto& kv : printers_.at(printer_class)) {
    const Printer& printer = kv.second;
    result.push_back(printer);
  }

  return result;
}

std::vector<Printer> PrintersMap::Get() const {
  std::vector<Printer> result;
  for (const auto& outer_map_entry : printers_) {
    const auto& printer_class = outer_map_entry.second;
    for (const auto& inner_map_entry : printer_class) {
      const Printer& printer = inner_map_entry.second;
      result.push_back(printer);
    }
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

    absl::optional<CupsPrinterStatus> printer_status =
        GetPrinterStatus(printer.id());
    if (printer_status) {
      Insert(printer_class, printer, printer_status.value());
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
  for (const auto& kv : printers_) {
    const PrinterClass& printer_class = kv.first;
    auto printers = GetSecurePrinters(printer_class);
    result.insert(result.end(), std::make_move_iterator(printers.begin()),
                  std::make_move_iterator(printers.end()));
  }

  return result;
}

std::vector<Printer> PrintersMap::GetSecurePrinters(
    PrinterClass printer_class) const {
  if (!HasPrintersInClass(printer_class)) {
    return std::vector<Printer>();
  }

  std::vector<Printer> result;
  result.reserve(printers_.at(printer_class).size());
  for (const auto& kv : printers_.at(printer_class)) {
    const Printer& printer = kv.second;
    if (printer.HasSecureProtocol()) {
      result.push_back(printer);
    }
  }

  return result;
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

bool PrintersMap::HasPrintersInClass(PrinterClass printer_class) const {
  return base::Contains(printers_, printer_class);
}

bool PrintersMap::IsPrinterInClass(PrinterClass printer_class,
                                   const std::string& printer_id) const {
  return HasPrintersInClass(printer_class) &&
         base::Contains(printers_.at(printer_class), printer_id);
}

bool PrintersMap::IsExistingPrinter(const std::string& printer_id) const {
  return Get(printer_id).has_value();
}

void PrintersMap::SavePrinterStatus(
    const std::string& printer_id,
    const CupsPrinterStatus& cups_printer_status) {
  printer_statuses_[printer_id] = cups_printer_status;

  for (auto& kv : printers_) {
    std::unordered_map<std::string, Printer>& printers_map = kv.second;
    auto printer_iter = printers_map.find(printer_id);
    if (printer_iter != printers_map.end()) {
      printer_iter->second.set_printer_status(cups_printer_status);
      return;
    }
  }
}

absl::optional<CupsPrinterStatus> PrintersMap::GetPrinterStatus(
    const std::string& printer_id) const {
  auto printer_iter = printer_statuses_.find(printer_id);
  if (printer_iter != printer_statuses_.end()) {
    return printer_iter->second;
  }
  return absl::nullopt;
}

std::set<std::string> PrintersMap::GetPrinterIdsInClass(
    PrinterClass printer_class) const {
  std::set<std::string> result;
  if (HasPrintersInClass(printer_class)) {
    for (const auto& kv : printers_.at(printer_class)) {
      const Printer& printer = kv.second;
      result.insert(printer.id());
    }
  }
  return result;
}

}  // namespace ash

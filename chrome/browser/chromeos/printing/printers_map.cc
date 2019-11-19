// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printers_map.h"

#include "base/stl_util.h"

namespace chromeos {
namespace {

bool IsProtocolSecure(const Printer& printer) {
  return !printer.HasNetworkProtocol() ||
         printer.GetProtocol() == Printer::kIpps ||
         printer.GetProtocol() == Printer::kHttps;
}

}  // namespace

PrintersMap::PrintersMap() = default;
PrintersMap::~PrintersMap() = default;

base::Optional<Printer> PrintersMap::Get(const std::string& printer_id) const {
  for (const auto& kv : printers_) {
    const PrinterClass& printer_class = kv.first;
    const auto& printer_list = kv.second;
    if (IsPrinterInClass(printer_class, printer_id)) {
      return printer_list.at(printer_id);
    }
  }
  return base::nullopt;
}

base::Optional<Printer> PrintersMap::Get(PrinterClass printer_class,
                                         const std::string& printer_id) const {
  if (!IsPrinterInClass(printer_class, printer_id)) {
    return base::nullopt;
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

void PrintersMap::Clear(PrinterClass printer_class) {
  printers_[printer_class].clear();
}

void PrintersMap::ReplacePrintersInClass(PrinterClass printer_class,
                                         const std::vector<Printer>& printers) {
  Clear(printer_class);
  for (const auto& printer : printers) {
    Insert(printer_class, printer);
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
    if (IsProtocolSecure(printer)) {
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

}  // namespace chromeos

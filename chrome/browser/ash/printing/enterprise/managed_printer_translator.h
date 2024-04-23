// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_MANAGED_PRINTER_TRANSLATOR_H_
#define CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_MANAGED_PRINTER_TRANSLATOR_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/enterprise/managed_printer_configuration.pb.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

// Parses `config` into a `ManagedPrinterConfiguration` object. `config`
// represents one item in the printer configuration policy (See
// PrintersBulkConfiguration and DevicePrinters). Returns `std::nullopt` if
// `config` cannot be converted to a valid `ManagedPrinterConfiguration` proto
// message.
COMPONENT_EXPORT(CHROMEOS_PRINTING)
std::optional<ManagedPrinterConfiguration> ManagedPrinterConfigFromDict(
    const base::Value::Dict& config);

// Returns a new printer populated with the fields from `managed_printer` or
// `std::nullopt` if `managed_printer` does not represent a valid printer
// configuration.
COMPONENT_EXPORT(CHROMEOS_PRINTING)
std::optional<chromeos::Printer> PrinterFromManagedPrinterConfig(
    const ManagedPrinterConfiguration& managed_printer);

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_MANAGED_PRINTER_TRANSLATOR_H_

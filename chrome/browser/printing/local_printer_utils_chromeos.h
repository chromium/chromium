// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_LOCAL_PRINTER_UTILS_CHROMEOS_H_
#define CHROME_BROWSER_PRINTING_LOCAL_PRINTER_UTILS_CHROMEOS_H_

#include <optional>
#include <string>

#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/printer_configuration.h"

namespace printing {

struct PrinterSemanticCapsAndDefaults;

// Returns the platform-specific handle for the LocalPrinter interface.
crosapi::mojom::LocalPrinter* GetLocalPrinterInterface();

// The mojom LocalDestinationInfo object is a subset of the chromeos Printer
// object.
crosapi::mojom::LocalDestinationInfoPtr PrinterToMojom(
    const chromeos::Printer& printer);

// The mojom CapabilitiesResponse contains information about the chromeos
// Printer as well as its semantic capabilities.
crosapi::mojom::CapabilitiesResponsePtr PrinterWithCapabilitiesToMojom(
    const chromeos::Printer& printer,
    const std::optional<printing::PrinterSemanticCapsAndDefaults>& caps);

// The mojom PrinterStatus object contains all information in the
// CupsPrinterStatus object.
crosapi::mojom::PrinterStatusPtr StatusToMojom(
    const chromeos::CupsPrinterStatus& status);

// The mojom ManagedPrintOptions object contains print job options for a
// particular managed printer.
crosapi::mojom::ManagedPrintOptionsPtr ManagedPrintOptionsToMojom(
    const chromeos::Printer::ManagedPrintOptions& print_job_options);

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_LOCAL_PRINTER_UTILS_CHROMEOS_H_

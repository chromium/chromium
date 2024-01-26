// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_LOCAL_PRINTER_UTILS_CHROMEOS_H_
#define CHROME_BROWSER_PRINTING_LOCAL_PRINTER_UTILS_CHROMEOS_H_

#include <optional>
#include <string>

#include "chromeos/crosapi/mojom/local_printer.mojom.h"

namespace chromeos {
class CupsPrinterStatus;
class Printer;
}  // namespace chromeos

namespace printing {

class PrintedDocument;
class PrintJob;
struct PrinterSemanticCapsAndDefaults;

#if BUILDFLAG(IS_CHROMEOS_LACROS)

// Notify Ash Chrome of a new print job.
// If `local_printer` is null, this method fails.
void NotifyAshJobCreated(int job_id,
                         const PrintedDocument& document,
                         const crosapi::mojom::PrintJob::Source& source,
                         const std::string& source_id,
                         crosapi::mojom::LocalPrinter* local_printer);

// Same as above but gets the LocalPrinter from LacrosService.
void NotifyAshJobCreated(const PrintJob& job,
                         int job_id,
                         const PrintedDocument& document);

#endif

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

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_LOCAL_PRINTER_UTILS_CHROMEOS_H_

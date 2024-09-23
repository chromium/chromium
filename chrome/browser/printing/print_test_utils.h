// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_TEST_UTILS_H_
#define CHROME_BROWSER_PRINTING_PRINT_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom-forward.h"
#include "printing/print_settings.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class WebContents;
}

namespace printing::test {

extern const char kPrinterName[];
constexpr int kPrinterDpi = 600;

// Some common paper sizes that can be used for fake device configurations.
extern const PrinterSemanticCapsAndDefaults::Paper kPaperLetter;
extern const PrinterSemanticCapsAndDefaults::Paper kPaperLegal;

// Support values for `MakeDefaultPrintSettings()`.
constexpr int kPrinterDefaultRenderDpi = 72;
constexpr gfx::Size kPrinterCapabilitiesDpi(kPrinterDefaultRenderDpi,
                                            kPrinterDefaultRenderDpi);
constexpr int kPrintSettingsCopies = 42;
extern const std::vector<gfx::Size> kPrinterCapabilitiesDefaultDpis;
extern const PrinterBasicInfoOptions kPrintInfoOptions;

// Creates a print ticket with some default values. Based on ticket creation in
// chrome/browser/resources/print_preview/native_layer.js.
base::Value::Dict GetPrintTicket(mojom::PrinterType type);

// Make some settings which correspond to the defaults for the indicated
// printer.
std::unique_ptr<PrintSettings> MakeDefaultPrintSettings(
    const std::string& printer_name);

// Make some settings which are different than the generated defaults of
// `MakeDefaultPrintSettings()`, to be used if test calls
// `PrintingContext::AskUserForSettings()`.
std::unique_ptr<PrintSettings> MakeUserModifiedPrintSettings(
    const std::string& printer_name,
    const PageRanges* page_ranges);

// Simpler version of StartPrint() provided for convenience with the common
// defaults:
// - null `print_renderer`
// - `print_preview_disabled` set to false
// - `has_selection` set to false
void StartPrint(content::WebContents* web_contents);

}  // namespace printing::test

#endif  // CHROME_BROWSER_PRINTING_PRINT_TEST_UTILS_H_

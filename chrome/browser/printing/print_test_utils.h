// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_TEST_UTILS_H_
#define CHROME_BROWSER_PRINTING_PRINT_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "printing/backend/print_backend.h"
#include "printing/print_settings.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace mojom {
enum class PrinterType;
}

extern const char kDummyPrinterName[];
constexpr int kTestPrinterDpi = 600;

// Support values for `MakeDefaultPrintSettings()`.
constexpr int kTestPrinterDefaultRenderDpi = 72;
constexpr gfx::Size kTestPrinterCapabilitiesDpi(kTestPrinterDefaultRenderDpi,
                                                kTestPrinterDefaultRenderDpi);
constexpr int kTestPrintSettingsCopies = 42;
extern const std::vector<gfx::Size> kTestPrinterCapabilitiesDefaultDpis;
extern const PrinterBasicInfoOptions kTestDummyPrintInfoOptions;

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
    const std::string& printer_name);

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_TEST_UTILS_H_

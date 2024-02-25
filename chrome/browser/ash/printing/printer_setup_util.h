// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINTER_SETUP_UTIL_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINTER_SETUP_UTIL_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chromeos/printing/printer_configuration.h"
#include "printing/backend/print_backend.h"

namespace ash {
namespace printing {

using GetPrinterCapabilitiesCallback = base::OnceCallback<void(
    const std::optional<::printing::PrinterSemanticCapsAndDefaults>&)>;

// Sets up a printer (if necessary) and runs a callback with the printer
// capabilities once printer setup is complete. The callback is run
// regardless of whether or not the printer needed to be set up.
// This function must be called from the UI thread.
// This function is called when setting up a printer from Print Preview
// and records a metric with the printer setup result code.
void SetUpPrinter(CupsPrintersManager* printers_manager,
                  const chromeos::Printer& printer,
                  GetPrinterCapabilitiesCallback cb);

}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINTER_SETUP_UTIL_H_

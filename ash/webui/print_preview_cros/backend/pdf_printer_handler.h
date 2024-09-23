// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PRINT_PREVIEW_CROS_BACKEND_PDF_PRINTER_HANDLER_H_
#define ASH_WEBUI_PRINT_PREVIEW_CROS_BACKEND_PDF_PRINTER_HANDLER_H_

#include "ash/webui/print_preview_cros/mojom/printer_capabilities.mojom.h"

namespace ash::printing::print_preview {

// Handler responsible for providing PDF printing functionalities for CrOS
// Print Preview.
class PdfPrinterHandler {
 public:
  PdfPrinterHandler() = default;
  PdfPrinterHandler(const PdfPrinterHandler&) = delete;
  PdfPrinterHandler& operator=(const PdfPrinterHandler&) = delete;
  ~PdfPrinterHandler() = default;

  mojom::CapabilitiesPtr FetchCapabilities(const std::string& destination_id);
};

}  // namespace ash::printing::print_preview

#endif  // ASH_WEBUI_PRINT_PREVIEW_CROS_BACKEND_PDF_PRINTER_HANDLER_H_

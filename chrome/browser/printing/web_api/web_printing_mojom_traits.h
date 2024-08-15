// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_MOJOM_TRAITS_H_
#define CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_MOJOM_TRAITS_H_

#include <cups/ipp.h>

#include <memory>
#include <optional>

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "printing/print_settings.h"
#include "printing/printer_status.h"
#include "third_party/blink/public/mojom/printing/web_printing.mojom-forward.h"
#include "ui/gfx/geometry/size.h"

namespace mojo {

template <>
struct EnumTraits<blink::mojom::WebPrintingSides, printing::mojom::DuplexMode> {
  static blink::mojom::WebPrintingSides ToMojom(
      printing::mojom::DuplexMode input);
  static bool FromMojom(blink::mojom::WebPrintingSides input,
                        printing::mojom::DuplexMode* output);
};

template <>
struct EnumTraits<blink::mojom::WebPrinterState, ipp_pstate_t> {
  static blink::mojom::WebPrinterState ToMojom(ipp_pstate_t input);
  static bool FromMojom(blink::mojom::WebPrinterState input,
                        ipp_pstate_t* output) {
    NOTREACHED();
  }
};

template <>
struct EnumTraits<blink::mojom::WebPrinterStateReason,
                  printing::PrinterStatus::PrinterReason::Reason> {
  static blink::mojom::WebPrinterStateReason ToMojom(
      printing::PrinterStatus::PrinterReason::Reason input);
  static bool FromMojom(
      blink::mojom::WebPrinterStateReason input,
      printing::PrinterStatus::PrinterReason::Reason* output) {
    NOTREACHED();
  }
};

template <>
struct StructTraits<blink::mojom::WebPrintJobTemplateAttributesDataView,
                    std::unique_ptr<printing::PrintSettings>> {
  static bool IsNull(const std::unique_ptr<printing::PrintSettings>& ptr) {
    return !ptr;
  }
  static void SetToNull(std::unique_ptr<printing::PrintSettings>* output) {
    output->reset();
  }

  // All getters below are intentionally not implemented -- this is a one-way
  // typemap.
  static const std::string& job_name(
      const std::unique_ptr<printing::PrintSettings>& ptr) {
    NOTREACHED();
  }
  static uint32_t copies(const std::unique_ptr<printing::PrintSettings>& ptr) {
    NOTREACHED();
  }
  static const blink::mojom::WebPrintingMediaCollectionRequestedPtr& media_col(
      const std::unique_ptr<printing::PrintSettings>& ptr) {
    NOTREACHED();
  }
  static const std::optional<std::string>& media_source(
      const std::unique_ptr<printing::PrintSettings>& ptr) {
    NOTREACHED();
  }
  static const std::optional<blink::mojom::WebPrintingMultipleDocumentHandling>&
  multiple_document_handling(
      const std::unique_ptr<printing::PrintSettings>& ptr) {
    NOTREACHED();
  }
  static const std::optional<blink::mojom::WebPrintingOrientationRequested>&
  orientation_requested(const std::unique_ptr<printing::PrintSettings>& ptr) {
    NOTREACHED();
  }
  static const std::optional<gfx::Size>& printer_resolution(
      const std::unique_ptr<printing::PrintSettings>& ptr) {
    NOTREACHED();
  }
  static const std::optional<blink::mojom::WebPrintColorMode>& print_color_mode(
      const std::unique_ptr<printing::PrintSettings>& ptr) {
    NOTREACHED();
  }
  static const std::optional<blink::mojom::WebPrintingSides>& sides(
      const std::unique_ptr<printing::PrintSettings>& ptr) {
    NOTREACHED();
  }

  static bool Read(blink::mojom::WebPrintJobTemplateAttributesDataView data,
                   std::unique_ptr<printing::PrintSettings>* out);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_MOJOM_TRAITS_H_

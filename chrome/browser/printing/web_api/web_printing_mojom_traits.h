// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_MOJOM_TRAITS_H_
#define CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_MOJOM_TRAITS_H_

#include <memory>
#include <optional>

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "printing/print_settings.h"
#include "third_party/blink/public/mojom/printing/web_printing.mojom-forward.h"

namespace mojo {

template <>
struct EnumTraits<blink::mojom::WebPrintingSides, printing::mojom::DuplexMode> {
  static blink::mojom::WebPrintingSides ToMojom(
      printing::mojom::DuplexMode input);
  static bool FromMojom(blink::mojom::WebPrintingSides input,
                        printing::mojom::DuplexMode* output);
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
    NOTREACHED_NORETURN();
  }
  static uint32_t copies(const std::unique_ptr<printing::PrintSettings>& ptr) {
    NOTREACHED_NORETURN();
  }
  static const std::optional<blink::mojom::WebPrintingMultipleDocumentHandling>&
  multiple_document_handling(
      const std::unique_ptr<printing::PrintSettings>& ptr) {
    NOTREACHED_NORETURN();
  }
  static const std::optional<blink::mojom::WebPrintingSides>& sides(
      const std::unique_ptr<printing::PrintSettings>& ptr) {
    NOTREACHED_NORETURN();
  }

  static bool Read(blink::mojom::WebPrintJobTemplateAttributesDataView data,
                   std::unique_ptr<printing::PrintSettings>* out);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_MOJOM_TRAITS_H_

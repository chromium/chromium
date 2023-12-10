// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/web_api/web_printing_mojom_traits.h"

#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/blink/public/mojom/printing/web_printing.mojom.h"

namespace mojo {

namespace {
// sides:
using blink::mojom::WebPrintingSides;
using printing::mojom::DuplexMode;

// multiple-document-handling:
using MultipleDocumentHandling =
    blink::mojom::WebPrintingMultipleDocumentHandling;
}  // namespace

// static
blink::mojom::WebPrintingSides
EnumTraits<WebPrintingSides, DuplexMode>::ToMojom(
    printing::mojom::DuplexMode input) {
  switch (input) {
    case DuplexMode::kSimplex:
      return WebPrintingSides::kOneSided;
    case DuplexMode::kLongEdge:
      return WebPrintingSides::kTwoSidedLongEdge;
    case DuplexMode::kShortEdge:
      return WebPrintingSides::kTwoSidedShortEdge;
    case DuplexMode::kUnknownDuplexMode:
      NOTREACHED_NORETURN();
  }
}

// static
bool EnumTraits<WebPrintingSides, DuplexMode>::FromMojom(WebPrintingSides input,
                                                         DuplexMode* output) {
  switch (input) {
    case WebPrintingSides::kOneSided:
      *output = DuplexMode::kSimplex;
      return true;
    case WebPrintingSides::kTwoSidedLongEdge:
      *output = DuplexMode::kLongEdge;
      return true;
    case WebPrintingSides::kTwoSidedShortEdge:
      *output = DuplexMode::kShortEdge;
      return true;
  }
}

// static
bool StructTraits<blink::mojom::WebPrintJobTemplateAttributesDataView,
                  std::unique_ptr<printing::PrintSettings>>::
    Read(blink::mojom::WebPrintJobTemplateAttributesDataView data,
         std::unique_ptr<printing::PrintSettings>* out) {
  auto settings = std::make_unique<printing::PrintSettings>();

  settings->set_copies(data.copies());
  {
    std::string job_name;
    if (!data.ReadJobName(&job_name)) {
      return false;
    }
    settings->set_title(base::UTF8ToUTF16(job_name));
  }
  {
    std::optional<DuplexMode> duplex_mode;
    if (!data.ReadSides(&duplex_mode)) {
      return false;
    }
    if (duplex_mode) {
      settings->set_duplex_mode(*duplex_mode);
    }
  }
  if (auto mdh = data.multiple_document_handling()) {
    switch (*mdh) {
      case MultipleDocumentHandling::kSeparateDocumentsCollatedCopies:
        settings->set_collate(true);
        break;
      case MultipleDocumentHandling::kSeparateDocumentsUncollatedCopies:
        settings->set_collate(false);
        break;
    }
  }

  *out = std::move(settings);
  return true;
}

}  // namespace mojo

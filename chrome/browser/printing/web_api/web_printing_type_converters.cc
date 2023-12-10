// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/web_api/web_printing_type_converters.h"

#include <optional>

#include "base/containers/contains.h"
#include "mojo/public/cpp/bindings/message.h"
#include "printing/backend/print_backend.h"
#include "third_party/blink/public/mojom/printing/web_printing.mojom.h"

namespace printing {
namespace {

// Checks if enum-default and enum-supported are in sync.
template <typename T>
bool ValidateDefaultAgainstSupported(const std::optional<T>& enum_default,
                                     const std::vector<T>& enum_supported) {
  if (!enum_default && enum_supported.empty()) {
    // If both are empty, the invariant is fulfilled.
    return true;
  } else if (enum_default && !enum_supported.empty()) {
    // If both are non-empty, then the latter must contain the former.
    return base::Contains(enum_supported, *enum_default);
  } else {
    // If only one is populated, then there are some values that we don't
    // support.
    return false;
  }
}

void ProcessCopies(const PrinterSemanticCapsAndDefaults& caps,
                   blink::mojom::WebPrinterAttributes* attributes) {
  attributes->copies_default = 1;
  attributes->copies_supported =
      blink::mojom::WebPrintingRange::New(1, caps.copies_max);
}

void ProcessMultipleDocumentHandling(
    const PrinterSemanticCapsAndDefaults& caps,
    blink::mojom::WebPrinterAttributes* attributes) {
  attributes->multiple_document_handling_default =
      caps.collate_capable && caps.collate_default
          ? blink::mojom::WebPrintingMultipleDocumentHandling::
                kSeparateDocumentsCollatedCopies
          : blink::mojom::WebPrintingMultipleDocumentHandling::
                kSeparateDocumentsUncollatedCopies;
  attributes->multiple_document_handling_supported.push_back(
      blink::mojom::WebPrintingMultipleDocumentHandling::
          kSeparateDocumentsUncollatedCopies);
  if (caps.collate_capable) {
    attributes->multiple_document_handling_supported.push_back(
        blink::mojom::WebPrintingMultipleDocumentHandling::
            kSeparateDocumentsCollatedCopies);
  }
}

void ProcessSides(const PrinterSemanticCapsAndDefaults& caps,
                  blink::mojom::WebPrinterAttributes* attributes) {
  if (caps.duplex_default != mojom::DuplexMode::kUnknownDuplexMode) {
    attributes->sides_default = caps.duplex_default;
  }
  for (const auto& duplex : caps.duplex_modes) {
    if (duplex == mojom::DuplexMode::kUnknownDuplexMode) {
      mojo::ReportBadMessage("Unknown duplex enum value in duplex_modes!");
      return;
    }
    attributes->sides_supported.push_back(duplex);
  }

  if (!ValidateDefaultAgainstSupported(attributes->sides_default,
                                       attributes->sides_supported)) {
    attributes->sides_default.reset();
    attributes->sides_supported.clear();
    return;
  }
}

}  // namespace
}  // namespace printing

namespace mojo {

blink::mojom::WebPrinterAttributesPtr
TypeConverter<blink::mojom::WebPrinterAttributesPtr,
              printing::PrinterSemanticCapsAndDefaults>::
    Convert(const printing::PrinterSemanticCapsAndDefaults& capabilities) {
  auto attributes = blink::mojom::WebPrinterAttributes::New();

  printing::ProcessCopies(capabilities, attributes.get());
  printing::ProcessMultipleDocumentHandling(capabilities, attributes.get());
  printing::ProcessSides(capabilities, attributes.get());

  return attributes;
}

}  // namespace mojo

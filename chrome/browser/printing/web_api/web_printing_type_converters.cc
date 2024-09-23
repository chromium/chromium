// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/web_api/web_printing_type_converters.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "chrome/browser/printing/web_api/web_printing_utils.h"
#include "chrome/common/printing/print_media_l10n.h"
#include "mojo/public/cpp/bindings/message.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/backend/print_backend.h"
#include "printing/units.h"
#include "third_party/blink/public/mojom/printing/web_printing.mojom.h"

namespace {

using MediaCollection = blink::mojom::WebPrintingMediaCollection;
using MediaCollectionPtr = blink::mojom::WebPrintingMediaCollectionPtr;
using Paper = printing::PrinterSemanticCapsAndDefaults::Paper;

using MediaSize = blink::mojom::WebPrintingMediaSize;
using MediaSizeDimension = blink::mojom::WebPrintingMediaSizeDimension;
using Range = blink::mojom::WebPrintingRange;

}  // namespace

namespace mojo {

template <>
struct TypeConverter<MediaCollectionPtr, Paper> {
  static MediaCollectionPtr Convert(const Paper& paper) {
    // The printing subsystem operates on microns, whereas the spec-compliant
    // `media_size` must be represented in hundredths of millimeters (PWG
    // units).
    auto media_size = MediaSize::New();
    if (paper.SupportsCustomSize()) {
      media_size->y_dimension = MediaSizeDimension::NewRange(
          Range::New(paper.size_um().height() / printing::kMicronsPerPwgUnit,
                     paper.max_height_um() / printing::kMicronsPerPwgUnit));
    } else {
      media_size->y_dimension = MediaSizeDimension::NewValue(
          paper.size_um().height() / printing::kMicronsPerPwgUnit);
    }
    // `Paper` struct doesn't provide information about variable width
    // (moreover, papers with variable width are silently discarded by
    // ChromeOS). Hence `x_dimension` is always a single value.
    media_size->x_dimension = MediaSizeDimension::NewValue(
        paper.size_um().width() / printing::kMicronsPerPwgUnit);

    auto media_col = MediaCollection::New();
    media_col->media_size = std::move(media_size);
    media_col->media_size_name =
        printing::LocalizePaperDisplayName(paper.size_um()).vendor_id;
    return media_col;
  }
};

}  // namespace mojo

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

void ProcessMediaCollection(const PrinterSemanticCapsAndDefaults& caps,
                            blink::mojom::WebPrinterAttributes* attributes) {
  attributes->media_col_default =
      mojo::ConvertTo<MediaCollectionPtr>(caps.default_paper);
  attributes->media_col_database =
      mojo::ConvertTo<std::vector<MediaCollectionPtr>>(caps.papers);
}

void ProcessMediaSource(const PrinterSemanticCapsAndDefaults& caps,
                        blink::mojom::WebPrinterAttributes* attributes) {
  auto* media_source = internal::FindAdvancedCapability(caps, kIppMediaSource);
  if (!media_source) {
    return;
  }
  if (!media_source->default_value.empty()) {
    attributes->media_source_default = media_source->default_value;
  }
  if (!media_source->values.empty()) {
    attributes->media_source_supported =
        base::ToVector(media_source->values, &AdvancedCapabilityValue::name);
  }
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

void ProcessOrientationRequested(
    const PrinterSemanticCapsAndDefaults& caps,
    blink::mojom::WebPrinterAttributes* attributes) {
  // The assumptions below hold true for almost all modern printers, so this
  // yields a fine approximation even without further plumbing.
  // TODO(b/302505962): Consider querying the printer for reverse-portrait and
  // reverse-landscape.
  attributes->orientation_requested_default =
      blink::mojom::WebPrintingOrientationRequested::kPortrait;
  attributes->orientation_requested_supported = {
      blink::mojom::WebPrintingOrientationRequested::kPortrait,
      blink::mojom::WebPrintingOrientationRequested::kLandscape};
}

void ProcessPrinterResolution(const PrinterSemanticCapsAndDefaults& caps,
                              blink::mojom::WebPrinterAttributes* attributes) {
  attributes->printer_resolution_default = caps.default_dpi;
  attributes->printer_resolution_supported = caps.dpis;
}

void ProcessPrintColorMode(const PrinterSemanticCapsAndDefaults& caps,
                           blink::mojom::WebPrinterAttributes* attributes) {
  attributes->print_color_mode_default =
      caps.color_default ? blink::mojom::WebPrintColorMode::kColor
                         : blink::mojom::WebPrintColorMode::kMonochrome;
  attributes->print_color_mode_supported.push_back(
      blink::mojom::WebPrintColorMode::kMonochrome);
  if (caps.color_model != mojom::ColorModel::kUnknownColorModel) {
    attributes->print_color_mode_supported.push_back(
        blink::mojom::WebPrintColorMode::kColor);
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
  printing::ProcessMediaCollection(capabilities, attributes.get());
  printing::ProcessMediaSource(capabilities, attributes.get());
  printing::ProcessMultipleDocumentHandling(capabilities, attributes.get());
  printing::ProcessOrientationRequested(capabilities, attributes.get());
  printing::ProcessPrinterResolution(capabilities, attributes.get());
  printing::ProcessPrintColorMode(capabilities, attributes.get());
  printing::ProcessSides(capabilities, attributes.get());

  return attributes;
}

}  // namespace mojo

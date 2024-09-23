// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/mojom/print_mojom_traits.h"

#include "base/strings/stringprintf.h"
#include "printing/mojom/print.mojom.h"
#include "printing/units.h"

namespace mojo {

namespace {

// Transform paper size to Mojo type.
arc::mojom::PrintMediaSizePtr ToMediaSize(
    const printing::PrinterSemanticCapsAndDefaults::Paper& paper) {
  gfx::Size size_mil =
      gfx::ScaleToRoundedSize(paper.size_um(), 1.0f / printing::kMicronsPerMil);
  return arc::mojom::PrintMediaSize::New(paper.vendor_id(),
                                         paper.display_name(), size_mil.width(),
                                         size_mil.height());
}

arc::mojom::PrintDuplexMode ToArcDuplexMode(printing::mojom::DuplexMode mode) {
  switch (mode) {
    case printing::mojom::DuplexMode::kLongEdge:
      return arc::mojom::PrintDuplexMode::LONG_EDGE;
    case printing::mojom::DuplexMode::kShortEdge:
      return arc::mojom::PrintDuplexMode::SHORT_EDGE;
    case printing::mojom::DuplexMode::kSimplex:
      return arc::mojom::PrintDuplexMode::NONE;
    default:
      NOTREACHED();
  }
}

}  // namespace

std::string StructTraits<arc::mojom::PrintResolutionDataView, gfx::Size>::id(
    const gfx::Size& size) {
  return base::StringPrintf("%dx%d", size.width(), size.height());
}

std::vector<arc::mojom::PrintMediaSizePtr>
StructTraits<arc::mojom::PrinterCapabilitiesDataView,
             printing::PrinterSemanticCapsAndDefaults>::
    media_sizes(const printing::PrinterSemanticCapsAndDefaults& caps) {
  std::vector<arc::mojom::PrintMediaSizePtr> sizes;
  sizes.reserve(caps.papers.size());
  for (const auto& paper : caps.papers)
    sizes.emplace_back(ToMediaSize(paper));

  return sizes;
}

arc::mojom::PrintMarginsPtr
StructTraits<arc::mojom::PrinterCapabilitiesDataView,
             printing::PrinterSemanticCapsAndDefaults>::
    min_margins(const printing::PrinterSemanticCapsAndDefaults& caps) {
  return arc::mojom::PrintMargins::New(0, 0, 0, 0);
}

arc::mojom::PrintColorMode
StructTraits<arc::mojom::PrinterCapabilitiesDataView,
             printing::PrinterSemanticCapsAndDefaults>::
    color_modes(const printing::PrinterSemanticCapsAndDefaults& caps) {
  auto color_modes = static_cast<arc::mojom::PrintColorMode>(0);
  if (caps.bw_model != printing::mojom::ColorModel::kUnknownColorModel) {
    color_modes = static_cast<arc::mojom::PrintColorMode>(
        static_cast<uint32_t>(color_modes) |
        static_cast<uint32_t>(arc::mojom::PrintColorMode::MONOCHROME));
  }
  if (caps.color_model != printing::mojom::ColorModel::kUnknownColorModel) {
    color_modes = static_cast<arc::mojom::PrintColorMode>(
        static_cast<uint32_t>(color_modes) |
        static_cast<uint32_t>(arc::mojom::PrintColorMode::COLOR));
  }
  return color_modes;
}

arc::mojom::PrintDuplexMode
StructTraits<arc::mojom::PrinterCapabilitiesDataView,
             printing::PrinterSemanticCapsAndDefaults>::
    duplex_modes(const printing::PrinterSemanticCapsAndDefaults& caps) {
  uint32_t duplex_modes = 0;
  for (printing::mojom::DuplexMode mode : caps.duplex_modes) {
    duplex_modes |= static_cast<uint32_t>(ToArcDuplexMode(mode));
  }
  return static_cast<arc::mojom::PrintDuplexMode>(duplex_modes);
}

arc::mojom::PrintAttributesPtr
StructTraits<arc::mojom::PrinterCapabilitiesDataView,
             printing::PrinterSemanticCapsAndDefaults>::
    defaults(const printing::PrinterSemanticCapsAndDefaults& caps) {
  arc::mojom::PrintDuplexMode default_duplex_mode =
      ToArcDuplexMode(caps.duplex_default);
  return arc::mojom::PrintAttributes::New(
      ToMediaSize(caps.default_paper), caps.default_dpi,
      arc::mojom::PrintMargins::New(0, 0, 0, 0),
      caps.color_default ? arc::mojom::PrintColorMode::COLOR
                         : arc::mojom::PrintColorMode::MONOCHROME,
      default_duplex_mode);
}

}  // namespace mojo

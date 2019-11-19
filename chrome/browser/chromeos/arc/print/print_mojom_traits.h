// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_PRINT_PRINT_MOJOM_TRAITS_H_
#define CHROME_BROWSER_CHROMEOS_ARC_PRINT_PRINT_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "components/arc/mojom/print_common.mojom.h"
#include "printing/backend/print_backend.h"
#include "printing/page_range.h"
#include "ui/gfx/geometry/size.h"

namespace mojo {

template <>
struct StructTraits<arc::mojom::PrintPageRangeDataView, printing::PageRange> {
  static uint32_t start(const printing::PageRange& r) { return r.from; }
  static uint32_t end(const printing::PageRange& r) { return r.to; }

  static bool Read(arc::mojom::PrintPageRangeDataView data,
                   printing::PageRange* out) {
    out->from = data.start();
    out->to = data.end();
    return true;
  }
};

template <>
struct StructTraits<arc::mojom::PrintResolutionDataView, gfx::Size> {
  static uint32_t horizontal_dpi(const gfx::Size& size) { return size.width(); }
  static uint32_t vertical_dpi(const gfx::Size& size) { return size.width(); }
  static std::string id(const gfx::Size& size);
  static std::string label(const gfx::Size& size) { return id(size); }

  static bool Read(arc::mojom::PrintResolutionDataView data, gfx::Size* out) {
    *out = gfx::Size(data.horizontal_dpi(), data.vertical_dpi());
    return true;
  }
};

// TODO(vkuzkokov): PrinterSemanticCapsAndDefaults has no margins, boolean
// duplex_capable, and unlabeled resolutions.
template <>
struct StructTraits<arc::mojom::PrinterCapabilitiesDataView,
                    printing::PrinterSemanticCapsAndDefaults> {
  static std::vector<arc::mojom::PrintMediaSizePtr> media_sizes(
      const printing::PrinterSemanticCapsAndDefaults& caps);

  static const std::vector<gfx::Size>& resolutions(
      const printing::PrinterSemanticCapsAndDefaults& caps) {
    return caps.dpis;
  }

  static arc::mojom::PrintMarginsPtr min_margins(
      const printing::PrinterSemanticCapsAndDefaults& caps);

  static arc::mojom::PrintColorMode color_modes(
      const printing::PrinterSemanticCapsAndDefaults& caps);

  static arc::mojom::PrintDuplexMode duplex_modes(
      const printing::PrinterSemanticCapsAndDefaults& caps);

  static arc::mojom::PrintAttributesPtr defaults(
      const printing::PrinterSemanticCapsAndDefaults& caps);

  static bool Read(arc::mojom::PrinterCapabilitiesDataView data,
                   printing::PrinterSemanticCapsAndDefaults* out) {
    // This is never used.
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_CHROMEOS_ARC_PRINT_PRINT_MOJOM_TRAITS_H_

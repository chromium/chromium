// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef ASH_COMPONENTS_ARC_BITMAP_BITMAP_MOJOM_TRAITS_H_
#define ASH_COMPONENTS_ARC_BITMAP_BITMAP_MOJOM_TRAITS_H_

#include "ash/components/arc/mojom/bitmap.mojom-shared.h"
#include "base/containers/span.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace mojo {

template <>
struct StructTraits<arc::mojom::ArcBitmapDataView, SkBitmap> {
  static const base::span<const uint8_t> pixel_data(const SkBitmap& r) {
    const SkImageInfo& info = r.info();
    DCHECK_EQ(info.colorType(), kRGBA_8888_SkColorType);

    return base::make_span(static_cast<uint8_t*>(r.getPixels()),
                           r.computeByteSize());
  }
  static uint32_t width(const SkBitmap& r) { return r.width(); }
  static uint32_t height(const SkBitmap& r) { return r.height(); }

  static bool Read(arc::mojom::ArcBitmapDataView data, SkBitmap* out);
};

}  // namespace mojo

#endif  // ASH_COMPONENTS_ARC_BITMAP_BITMAP_MOJOM_TRAITS_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_PAINT_FLAGS_MOJOM_TRAITS_H_
#define CC_MOJOM_PAINT_FLAGS_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "cc/mojom/paint_flags.mojom-shared.h"
#include "cc/paint/paint_flags.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<cc::mojom::FilterQuality, cc::PaintFlags::FilterQuality> {
  static cc::mojom::FilterQuality ToMojom(cc::PaintFlags::FilterQuality input) {
    switch (input) {
      case cc::PaintFlags::FilterQuality::kNone:
        return cc::mojom::FilterQuality::kNone;
      case cc::PaintFlags::FilterQuality::kLow:
        return cc::mojom::FilterQuality::kLow;
      case cc::PaintFlags::FilterQuality::kMedium:
        return cc::mojom::FilterQuality::kMedium;
      case cc::PaintFlags::FilterQuality::kHigh:
        return cc::mojom::FilterQuality::kHigh;
    }
    NOTREACHED();
  }

  static cc::PaintFlags::FilterQuality FromMojom(
      cc::mojom::FilterQuality input) {
    switch (input) {
      case cc::mojom::FilterQuality::kNone:
        return cc::PaintFlags::FilterQuality::kNone;
      case cc::mojom::FilterQuality::kLow:
        return cc::PaintFlags::FilterQuality::kLow;
      case cc::mojom::FilterQuality::kMedium:
        return cc::PaintFlags::FilterQuality::kMedium;
      case cc::mojom::FilterQuality::kHigh:
        return cc::PaintFlags::FilterQuality::kHigh;
    }
    NOTREACHED();
  }
};

template <>
struct StructTraits<cc::mojom::DynamicRangeLimitDataView,
                    cc::PaintFlags::DynamicRangeLimitMixture> {
  static float standard_mix(const cc::PaintFlags::DynamicRangeLimitMixture& m) {
    return m.standard_mix;
  }
  static float constrained_high_mix(
      const cc::PaintFlags::DynamicRangeLimitMixture& m) {
    return m.constrained_high_mix;
  }

  static bool Read(cc::mojom::DynamicRangeLimitDataView data,
                   cc::PaintFlags::DynamicRangeLimitMixture* out) {
    out->standard_mix = data.standard_mix();
    out->constrained_high_mix = data.constrained_high_mix();
    return true;
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_PAINT_FLAGS_MOJOM_TRAITS_H_

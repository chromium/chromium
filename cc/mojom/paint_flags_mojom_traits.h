// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_PAINT_FLAGS_MOJOM_TRAITS_H_
#define CC_MOJOM_PAINT_FLAGS_MOJOM_TRAITS_H_

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

  static bool FromMojom(cc::mojom::FilterQuality input,
                        cc::PaintFlags::FilterQuality* out) {
    switch (input) {
      case cc::mojom::FilterQuality::kNone:
        *out = cc::PaintFlags::FilterQuality::kNone;
        return true;
      case cc::mojom::FilterQuality::kLow:
        *out = cc::PaintFlags::FilterQuality::kLow;
        return true;
      case cc::mojom::FilterQuality::kMedium:
        *out = cc::PaintFlags::FilterQuality::kMedium;
        return true;
      case cc::mojom::FilterQuality::kHigh:
        *out = cc::PaintFlags::FilterQuality::kHigh;
        return true;
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

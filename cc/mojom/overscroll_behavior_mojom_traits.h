// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_OVERSCROLL_BEHAVIOR_MOJOM_TRAITS_H_
#define CC_MOJOM_OVERSCROLL_BEHAVIOR_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/mojom/overscroll_behavior.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<cc::mojom::OverscrollBehaviorType,
                  cc::OverscrollBehavior::Type> {
  static cc::mojom::OverscrollBehaviorType ToMojom(
      cc::OverscrollBehavior::Type input) {
    switch (input) {
      case cc::OverscrollBehavior::Type::kNone:
        return cc::mojom::OverscrollBehaviorType::kNone;
      case cc::OverscrollBehavior::Type::kAuto:
        return cc::mojom::OverscrollBehaviorType::kAuto;
      case cc::OverscrollBehavior::Type::kContain:
        return cc::mojom::OverscrollBehaviorType::kContain;
      case cc::OverscrollBehavior::Type::kChain:
        return cc::mojom::OverscrollBehaviorType::kChain;
      default:
        NOTREACHED();
    }
  }

  static cc::OverscrollBehavior::Type FromMojom(
      cc::mojom::OverscrollBehaviorType input) {
    switch (input) {
      case cc::mojom::OverscrollBehaviorType::kNone:
        return cc::OverscrollBehavior::Type::kNone;
      case cc::mojom::OverscrollBehaviorType::kAuto:
        return cc::OverscrollBehavior::Type::kAuto;
      case cc::mojom::OverscrollBehaviorType::kContain:
        return cc::OverscrollBehavior::Type::kContain;
      case cc::mojom::OverscrollBehaviorType::kChain:
        return cc::OverscrollBehavior::Type::kChain;
    }
    NOTREACHED();
  }
};

template <>
struct StructTraits<cc::mojom::OverscrollBehaviorDataView,
                    cc::OverscrollBehavior> {
  static cc::OverscrollBehavior::Type x(const cc::OverscrollBehavior& input) {
    return input.x;
  }

  static cc::OverscrollBehavior::Type y(const cc::OverscrollBehavior& input) {
    return input.y;
  }

  static bool Read(cc::mojom::OverscrollBehaviorDataView data,
                   cc::OverscrollBehavior* out) {
    return data.ReadX(&out->x) && data.ReadY(&out->y);
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_OVERSCROLL_BEHAVIOR_MOJOM_TRAITS_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_SCROLLBAR_ANIMATOR_MOJOM_TRAITS_H_
#define CC_MOJOM_SCROLLBAR_ANIMATOR_MOJOM_TRAITS_H_

#include "cc/mojom/scrollbar_animator.mojom-shared.h"
#include "cc/trees/layer_tree_settings.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct EnumTraits<cc::mojom::ScrollbarAnimator,
                  cc::LayerTreeSettings::ScrollbarAnimator> {
  static cc::mojom::ScrollbarAnimator ToMojom(
      cc::LayerTreeSettings::ScrollbarAnimator input) {
    switch (input) {
      case cc::LayerTreeSettings::ScrollbarAnimator::NO_ANIMATOR:
        return cc::mojom::ScrollbarAnimator::kNoAnimator;
      case cc::LayerTreeSettings::ScrollbarAnimator::ANDROID_OVERLAY:
        return cc::mojom::ScrollbarAnimator::kAndroidOverlay;
      case cc::LayerTreeSettings::ScrollbarAnimator::AURA_OVERLAY:
        return cc::mojom::ScrollbarAnimator::kAuraOverlay;
    }
    NOTREACHED();
  }

  static bool FromMojom(cc::mojom::ScrollbarAnimator input,
                        cc::LayerTreeSettings::ScrollbarAnimator* out) {
    switch (input) {
      case cc::mojom::ScrollbarAnimator::kNoAnimator:
        *out = cc::LayerTreeSettings::ScrollbarAnimator::NO_ANIMATOR;
        return true;
      case cc::mojom::ScrollbarAnimator::kAndroidOverlay:
        *out = cc::LayerTreeSettings::ScrollbarAnimator::ANDROID_OVERLAY;
        return true;
      case cc::mojom::ScrollbarAnimator::kAuraOverlay:
        *out = cc::LayerTreeSettings::ScrollbarAnimator::AURA_OVERLAY;
        return true;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_SCROLLBAR_ANIMATOR_MOJOM_TRAITS_H_

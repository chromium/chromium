// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_RENDER_SURFACE_REASON_MOJOM_TRAITS_H_
#define CC_MOJOM_RENDER_SURFACE_REASON_MOJOM_TRAITS_H_

#include "cc/mojom/render_surface_reason.mojom-shared.h"
#include "cc/trees/effect_node.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<cc::mojom::RenderSurfaceReason, cc::RenderSurfaceReason> {
  static cc::mojom::RenderSurfaceReason ToMojom(cc::RenderSurfaceReason input) {
    switch (input) {
      case cc::RenderSurfaceReason::kNone:
        return cc::mojom::RenderSurfaceReason::kNone;
      case cc::RenderSurfaceReason::kRoot:
        return cc::mojom::RenderSurfaceReason::kRoot;
      case cc::RenderSurfaceReason::k3dTransformFlattening:
        return cc::mojom::RenderSurfaceReason::k3dTransformFlattening;
      case cc::RenderSurfaceReason::kBackdropScope:
        return cc::mojom::RenderSurfaceReason::kBackdropScope;
      case cc::RenderSurfaceReason::kBlendMode:
        return cc::mojom::RenderSurfaceReason::kBlendMode;
      case cc::RenderSurfaceReason::kBlendModeDstIn:
        return cc::mojom::RenderSurfaceReason::kBlendModeDstIn;
      case cc::RenderSurfaceReason::kOpacity:
        return cc::mojom::RenderSurfaceReason::kOpacity;
      case cc::RenderSurfaceReason::kOpacityAnimation:
        return cc::mojom::RenderSurfaceReason::kOpacityAnimation;
      case cc::RenderSurfaceReason::kFilter:
        return cc::mojom::RenderSurfaceReason::kFilter;
      case cc::RenderSurfaceReason::kFilterAnimation:
        return cc::mojom::RenderSurfaceReason::kFilterAnimation;
      case cc::RenderSurfaceReason::kBackdropFilter:
        return cc::mojom::RenderSurfaceReason::kBackdropFilter;
      case cc::RenderSurfaceReason::kBackdropFilterAnimation:
        return cc::mojom::RenderSurfaceReason::kBackdropFilterAnimation;
      case cc::RenderSurfaceReason::kRoundedCorner:
        return cc::mojom::RenderSurfaceReason::kRoundedCorner;
      case cc::RenderSurfaceReason::kClipPath:
        return cc::mojom::RenderSurfaceReason::kClipPath;
      case cc::RenderSurfaceReason::kClipAxisAlignment:
        return cc::mojom::RenderSurfaceReason::kClipAxisAlignment;
      case cc::RenderSurfaceReason::kMask:
        return cc::mojom::RenderSurfaceReason::kMask;
      case cc::RenderSurfaceReason::kTrilinearFiltering:
        return cc::mojom::RenderSurfaceReason::kTrilinearFiltering;
      case cc::RenderSurfaceReason::kCache:
        return cc::mojom::RenderSurfaceReason::kCache;
      case cc::RenderSurfaceReason::kCopyRequest:
        return cc::mojom::RenderSurfaceReason::kCopyRequest;
      case cc::RenderSurfaceReason::kMirrored:
        return cc::mojom::RenderSurfaceReason::kMirrored;
      case cc::RenderSurfaceReason::kSubtreeIsBeingCaptured:
        return cc::mojom::RenderSurfaceReason::kSubtreeIsBeingCaptured;
      case cc::RenderSurfaceReason::kViewTransitionParticipant:
        return cc::mojom::RenderSurfaceReason::kViewTransitionParticipant;
      case cc::RenderSurfaceReason::kGradientMask:
        return cc::mojom::RenderSurfaceReason::kGradientMask;
      case cc::RenderSurfaceReason::k2DScaleTransformWithCompositedDescendants:
        return cc::mojom::RenderSurfaceReason::
            k2DScaleTransformWithCompositedDescendants;
      case cc::RenderSurfaceReason::kTest:
        return cc::mojom::RenderSurfaceReason::kTest;
    }
    NOTREACHED();
  }

  static bool FromMojom(cc::mojom::RenderSurfaceReason input,
                        cc::RenderSurfaceReason* out) {
    switch (input) {
      case cc::mojom::RenderSurfaceReason::kNone:
        *out = cc::RenderSurfaceReason::kNone;
        return true;
      case cc::mojom::RenderSurfaceReason::kRoot:
        *out = cc::RenderSurfaceReason::kRoot;
        return true;
      case cc::mojom::RenderSurfaceReason::k3dTransformFlattening:
        *out = cc::RenderSurfaceReason::k3dTransformFlattening;
        return true;
      case cc::mojom::RenderSurfaceReason::kBackdropScope:
        *out = cc::RenderSurfaceReason::kBackdropScope;
        return true;
      case cc::mojom::RenderSurfaceReason::kBlendMode:
        *out = cc::RenderSurfaceReason::kBlendMode;
        return true;
      case cc::mojom::RenderSurfaceReason::kBlendModeDstIn:
        *out = cc::RenderSurfaceReason::kBlendModeDstIn;
        return true;
      case cc::mojom::RenderSurfaceReason::kOpacity:
        *out = cc::RenderSurfaceReason::kOpacity;
        return true;
      case cc::mojom::RenderSurfaceReason::kOpacityAnimation:
        *out = cc::RenderSurfaceReason::kOpacityAnimation;
        return true;
      case cc::mojom::RenderSurfaceReason::kFilter:
        *out = cc::RenderSurfaceReason::kFilter;
        return true;
      case cc::mojom::RenderSurfaceReason::kFilterAnimation:
        *out = cc::RenderSurfaceReason::kFilterAnimation;
        return true;
      case cc::mojom::RenderSurfaceReason::kBackdropFilter:
        *out = cc::RenderSurfaceReason::kBackdropFilter;
        return true;
      case cc::mojom::RenderSurfaceReason::kBackdropFilterAnimation:
        *out = cc::RenderSurfaceReason::kBackdropFilterAnimation;
        return true;
      case cc::mojom::RenderSurfaceReason::kRoundedCorner:
        *out = cc::RenderSurfaceReason::kRoundedCorner;
        return true;
      case cc::mojom::RenderSurfaceReason::kClipPath:
        *out = cc::RenderSurfaceReason::kClipPath;
        return true;
      case cc::mojom::RenderSurfaceReason::kClipAxisAlignment:
        *out = cc::RenderSurfaceReason::kClipAxisAlignment;
        return true;
      case cc::mojom::RenderSurfaceReason::kMask:
        *out = cc::RenderSurfaceReason::kMask;
        return true;
      case cc::mojom::RenderSurfaceReason::kTrilinearFiltering:
        *out = cc::RenderSurfaceReason::kTrilinearFiltering;
        return true;
      case cc::mojom::RenderSurfaceReason::kCache:
        *out = cc::RenderSurfaceReason::kCache;
        return true;
      case cc::mojom::RenderSurfaceReason::kCopyRequest:
        *out = cc::RenderSurfaceReason::kCopyRequest;
        return true;
      case cc::mojom::RenderSurfaceReason::kMirrored:
        *out = cc::RenderSurfaceReason::kMirrored;
        return true;
      case cc::mojom::RenderSurfaceReason::kSubtreeIsBeingCaptured:
        *out = cc::RenderSurfaceReason::kSubtreeIsBeingCaptured;
        return true;
      case cc::mojom::RenderSurfaceReason::kViewTransitionParticipant:
        *out = cc::RenderSurfaceReason::kViewTransitionParticipant;
        return true;
      case cc::mojom::RenderSurfaceReason::kGradientMask:
        *out = cc::RenderSurfaceReason::kGradientMask;
        return true;
      case cc::mojom::RenderSurfaceReason::
          k2DScaleTransformWithCompositedDescendants:
        *out =
            cc::RenderSurfaceReason::k2DScaleTransformWithCompositedDescendants;
        return true;
      case cc::mojom::RenderSurfaceReason::kTest:
        *out = cc::RenderSurfaceReason::kTest;
        return true;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_RENDER_SURFACE_REASON_MOJOM_TRAITS_H_

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
      case cc::RenderSurfaceReason::kUnboundedElement:
        return cc::mojom::RenderSurfaceReason::kUnboundedElement;
      case cc::RenderSurfaceReason::kTest:
        return cc::mojom::RenderSurfaceReason::kTest;
    }
    NOTREACHED();
  }

  static cc::RenderSurfaceReason FromMojom(
      cc::mojom::RenderSurfaceReason input) {
    switch (input) {
      case cc::mojom::RenderSurfaceReason::kNone:
        return cc::RenderSurfaceReason::kNone;
      case cc::mojom::RenderSurfaceReason::kRoot:
        return cc::RenderSurfaceReason::kRoot;
      case cc::mojom::RenderSurfaceReason::k3dTransformFlattening:
        return cc::RenderSurfaceReason::k3dTransformFlattening;
      case cc::mojom::RenderSurfaceReason::kBackdropScope:
        return cc::RenderSurfaceReason::kBackdropScope;
      case cc::mojom::RenderSurfaceReason::kBlendMode:
        return cc::RenderSurfaceReason::kBlendMode;
      case cc::mojom::RenderSurfaceReason::kBlendModeDstIn:
        return cc::RenderSurfaceReason::kBlendModeDstIn;
      case cc::mojom::RenderSurfaceReason::kOpacity:
        return cc::RenderSurfaceReason::kOpacity;
      case cc::mojom::RenderSurfaceReason::kOpacityAnimation:
        return cc::RenderSurfaceReason::kOpacityAnimation;
      case cc::mojom::RenderSurfaceReason::kFilter:
        return cc::RenderSurfaceReason::kFilter;
      case cc::mojom::RenderSurfaceReason::kFilterAnimation:
        return cc::RenderSurfaceReason::kFilterAnimation;
      case cc::mojom::RenderSurfaceReason::kBackdropFilter:
        return cc::RenderSurfaceReason::kBackdropFilter;
      case cc::mojom::RenderSurfaceReason::kBackdropFilterAnimation:
        return cc::RenderSurfaceReason::kBackdropFilterAnimation;
      case cc::mojom::RenderSurfaceReason::kRoundedCorner:
        return cc::RenderSurfaceReason::kRoundedCorner;
      case cc::mojom::RenderSurfaceReason::kClipPath:
        return cc::RenderSurfaceReason::kClipPath;
      case cc::mojom::RenderSurfaceReason::kClipAxisAlignment:
        return cc::RenderSurfaceReason::kClipAxisAlignment;
      case cc::mojom::RenderSurfaceReason::kMask:
        return cc::RenderSurfaceReason::kMask;
      case cc::mojom::RenderSurfaceReason::kTrilinearFiltering:
        return cc::RenderSurfaceReason::kTrilinearFiltering;
      case cc::mojom::RenderSurfaceReason::kCache:
        return cc::RenderSurfaceReason::kCache;
      case cc::mojom::RenderSurfaceReason::kCopyRequest:
        return cc::RenderSurfaceReason::kCopyRequest;
      case cc::mojom::RenderSurfaceReason::kMirrored:
        return cc::RenderSurfaceReason::kMirrored;
      case cc::mojom::RenderSurfaceReason::kSubtreeIsBeingCaptured:
        return cc::RenderSurfaceReason::kSubtreeIsBeingCaptured;
      case cc::mojom::RenderSurfaceReason::kViewTransitionParticipant:
        return cc::RenderSurfaceReason::kViewTransitionParticipant;
      case cc::mojom::RenderSurfaceReason::kGradientMask:
        return cc::RenderSurfaceReason::kGradientMask;
      case cc::mojom::RenderSurfaceReason::
          k2DScaleTransformWithCompositedDescendants:
        return cc::RenderSurfaceReason::
            k2DScaleTransformWithCompositedDescendants;
      case cc::mojom::RenderSurfaceReason::kUnboundedElement:
        return cc::RenderSurfaceReason::kUnboundedElement;
      case cc::mojom::RenderSurfaceReason::kTest:
        return cc::RenderSurfaceReason::kTest;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_RENDER_SURFACE_REASON_MOJOM_TRAITS_H_

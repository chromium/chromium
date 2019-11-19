// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_
#define CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_

#include <memory>
#include <string>
#include "cc/cc_export.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace cc {

// Ensure this stays in sync with MainThreadScrollingReason in histograms.xml.
// When adding a new MainThreadScrollingReason, make sure the corresponding
// [MainThread/Compositor]CanSetScrollReasons function is also updated.
struct CC_EXPORT MainThreadScrollingReason {
  enum : uint32_t {
    // Non-transient scrolling reasons.
    kNotScrollingOnMain = 0,
    kHasBackgroundAttachmentFixedObjects = 1 << 0,
    kHasNonLayerViewportConstrainedObjects = 1 << 1,
    kThreadedScrollingDisabled = 1 << 2,
    kScrollbarScrolling = 1 << 3,
    kFrameOverlay = 1 << 4,

    // This bit is set when any of the other main thread scrolling reasons cause
    // an input event to be handled on the main thread, and the main thread
    // blink::ScrollAnimator is in the middle of running a scroll offset
    // animation. Note that a scroll handled by the main thread can result in an
    // animation running on the main thread or on the compositor thread.
    kHandlingScrollFromMainThread = 1 << 13,

    // Style-related scrolling on main reasons.
    // These *AndLCDText reasons are due to subpixel text rendering which can
    // only be applied by blending glyphs with the background at a specific
    // screen position; transparency and transforms break this.
    kNonCompositedReasonsFirst = 16,
    kHasOpacityAndLCDText = 1 << 16,
    kHasTransformAndLCDText = 1 << 17,
    kBackgroundNotOpaqueInRectAndLCDText = 1 << 18,
    kHasClipRelatedProperty = 1 << 20,
    kHasBoxShadowFromNonRootLayer = 1 << 21,
    kIsNotStackingContextAndLCDText = 1 << 22,
    kNonCompositedReasonsLast = 22,

    // Transient scrolling reasons. These are computed for each scroll begin.
    kNonFastScrollableRegion = 1 << 5,
    kFailedHitTest = 1 << 7,
    kNoScrollingLayer = 1 << 8,
    kNotScrollable = 1 << 9,
    kContinuingMainThreadScroll = 1 << 10,
    kNonInvertibleTransform = 1 << 11,
    kPageBasedScrolling = 1 << 12,
    kWheelEventHandlerRegion = 1 << 23,
    kTouchEventHandlerRegion = 1 << 24,

    // The maximum number of flags in this struct (excluding itself).
    // New flags should increment this number but it should never be decremented
    // because the values are used in UMA histograms. It should also be noted
    // that it excludes the kNotScrollingOnMain value.
    kMainThreadScrollingReasonCount = 25,
  };

  static const uint32_t kNonCompositedReasons =
      kHasOpacityAndLCDText | kHasTransformAndLCDText |
      kBackgroundNotOpaqueInRectAndLCDText | kHasClipRelatedProperty |
      kHasBoxShadowFromNonRootLayer | kIsNotStackingContextAndLCDText;

  // Returns true if the given MainThreadScrollingReason can be set by the main
  // thread.
  static bool MainThreadCanSetScrollReasons(uint32_t reasons) {
    uint32_t reasons_set_by_main_thread =
        kNotScrollingOnMain | kHasBackgroundAttachmentFixedObjects |
        kHasNonLayerViewportConstrainedObjects | kThreadedScrollingDisabled |
        kScrollbarScrolling | kFrameOverlay | kHandlingScrollFromMainThread;
    return (reasons & reasons_set_by_main_thread) == reasons;
  }

  // Returns true if the given MainThreadScrollingReason can be set by the
  // compositor.
  static bool CompositorCanSetScrollReasons(uint32_t reasons) {
    uint32_t reasons_set_by_compositor =
        kNonFastScrollableRegion | kFailedHitTest | kNoScrollingLayer |
        kNotScrollable | kContinuingMainThreadScroll | kNonInvertibleTransform |
        kPageBasedScrolling | kWheelEventHandlerRegion |
        kTouchEventHandlerRegion;
    return (reasons & reasons_set_by_compositor) == reasons;
  }

  // Returns true if there are any reasons that prevented the scroller
  // from being composited.
  static bool HasNonCompositedScrollReasons(uint32_t reasons) {
    return (reasons & kNonCompositedReasons) != 0;
  }

  static std::string AsText(uint32_t reasons);
  static void AddToTracedValue(uint32_t reasons,
                               base::trace_event::TracedValue&);
};

}  // namespace cc

#endif  // CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_

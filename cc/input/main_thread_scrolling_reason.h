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

// Ensure this stays in sync with MainThreadScrollingReason in
// tools/metrics/enums.xml. When adding a new MainThreadScrollingReason, make
// sure the corresponding [MainThread/Compositor]CanSetScrollReasons function
// is also updated.
struct CC_EXPORT MainThreadScrollingReason {
  enum : uint32_t {
    kNotScrollingOnMain = 0,

    // This enum simultaneously defines actual bitmask values and indices into
    // the bitmask, but kNotScrollingMain is recorded in the histograms as
    // value 0, so the 0th bit should never be used.
    // See also blink::RecordScrollReasonsMetric().

    // Non-transient scrolling reasons.
    kHasBackgroundAttachmentFixedObjects = 1 << 1,
    kThreadedScrollingDisabled = 1 << 3,

    // Style-related scrolling on main reasons.
    // These *AndLCDText reasons are due to subpixel text rendering which can
    // only be applied by blending glyphs with the background at a specific
    // screen position; transparency and transforms break this.
    kNonCompositedReasonsFirst = 18,
    kNotOpaqueForTextAndLCDText = 1 << 19,
    kCantPaintScrollingBackgroundAndLCDText = 1 << 20,
    kNonCompositedReasonsLast = 23,

    // Transient scrolling reasons. These are computed for each scroll begin.
    kScrollbarScrolling = 1 << 4,
    kNonFastScrollableRegion = 1 << 6,
    kFailedHitTest = 1 << 8,
    kNoScrollingLayer = 1 << 9,
    kNotScrollable = 1 << 10,
    kNonInvertibleTransform = 1 << 12,
    kWheelEventHandlerRegion = 1 << 24,
    kTouchEventHandlerRegion = 1 << 25,

    kMainThreadScrollingReasonLast = 25,
  };

  static const uint32_t kNonCompositedReasons =
      kNotOpaqueForTextAndLCDText | kCantPaintScrollingBackgroundAndLCDText;

  // Returns true if the given MainThreadScrollingReason can be set by the main
  // thread.
  static bool MainThreadCanSetScrollReasons(uint32_t reasons) {
    constexpr uint32_t reasons_set_by_main_thread =
        kHasBackgroundAttachmentFixedObjects | kThreadedScrollingDisabled;
    return (reasons & reasons_set_by_main_thread) == reasons;
  }

  // Returns true if the given MainThreadScrollingReason can be set by the
  // compositor.
  static bool CompositorCanSetScrollReasons(uint32_t reasons) {
    constexpr uint32_t reasons_set_by_compositor =
        kNonFastScrollableRegion | kFailedHitTest | kNoScrollingLayer |
        kNotScrollable | kNonInvertibleTransform | kWheelEventHandlerRegion |
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

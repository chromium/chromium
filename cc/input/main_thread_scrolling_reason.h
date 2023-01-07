// Copyright 2016 The Chromium Authors
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

// Ensure this stays in sync with the "MainThreadScrollingReason" enum in:
//   tools/metrics/histograms/enums.xml
// When adding a new MainThreadScrollingReason, make sure the corresponding
// [MainThread/Compositor]CanSetScrollReasons function is also updated.
//
// More info at: http://bit.ly/mtsr-details
//
struct CC_EXPORT MainThreadScrollingReason {
  enum : uint32_t {
    kNotScrollingOnMain = 0,

    // This is used only to report the histogram of main thread scrolling for
    // any reason below. It's a histogram bucket index instead of a bit.
    kScrollingOnMainForAnyReason = 1,

    // This enum simultaneously defines actual bitmask values and indices into
    // the bitmask (which are the numbers after "1 << " below, used as the
    // histogram bucket indices), but value 0 and 1 are used as the histogram
    // bucket indices for kNotScrollingMain and kScrollingOnMainForAnyReason,
    // respectively, so the 0th bit and the 1st bit should never be used.
    // See also blink::RecordScrollReasonsMetric().

    // Non-transient scrolling reasons. These are set on the ScrollNode.
    kHasBackgroundAttachmentFixedObjects = 1 << 2,
    kThreadedScrollingDisabled = 1 << 3,
    kPopupNoThreadedInput = 1 << 4,

    // Style-related scrolling on main reasons. Subpixel (LCD) text rendering
    // requires blending glyphs with the background at a specific screen
    // position; transparency and transforms break this.
    // These are only reported by the main-thread scroll gesture event codepath.
    // After scroll unification, we report kNoScrollingLayer instead.
    kNotOpaqueForTextAndLCDText = 1 << 5,
    kCantPaintScrollingBackgroundAndLCDText = 1 << 6,

    // Transient scrolling reasons. These are computed for each scroll gesture.
    // When computed inside ScrollBegin, these prevent the InputHandler from
    // reporting a status with SCROLL_ON_IMPL_THREAD. In other cases, the
    // InputHandler is scrolling "on impl", but we report a transient main
    // thread scrolling reason to UMA when we determine that some other aspect
    // of handling the scroll has been (or will be) blocked on the main thread.
    kScrollbarScrolling = 1 << 7,
    kNonFastScrollableRegion = 1 << 8,
    kFailedHitTest = 1 << 9,
    kNoScrollingLayer = 1 << 10,
    kNotScrollable = 1 << 11,
    kNonInvertibleTransform = 1 << 12,
    kWheelEventHandlerRegion = 1 << 13,
    kTouchEventHandlerRegion = 1 << 14,

    // For blink::RecordScrollReasonsMetric() to know the number of used bits.
    kMainThreadScrollingReasonLast = 14,
  };

  static const uint32_t kNonCompositedReasons =
      kNotOpaqueForTextAndLCDText | kCantPaintScrollingBackgroundAndLCDText;

  // Returns true if the given MainThreadScrollingReason can be set by the main
  // thread.
  static bool MainThreadCanSetScrollReasons(uint32_t reasons) {
    constexpr uint32_t reasons_set_by_main_thread =
        kHasBackgroundAttachmentFixedObjects | kThreadedScrollingDisabled |
        kPopupNoThreadedInput;
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

  static int BucketIndexForTesting(uint32_t reason);

  static std::string AsText(uint32_t reasons);
  static void AddToTracedValue(uint32_t reasons,
                               base::trace_event::TracedValue&);
};

}  // namespace cc

#endif  // CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_

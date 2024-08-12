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
// kRepaintReasons and kHitTestReasons are also updated.
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

    // Main-thread repaint reasons.
    // See InputHandler::ScrollStatus::main_thread_repaint_reasons.
    // They are set in ScrollNode::main_thread_scrolling_reasons, or
    // kNoScrollingLayer is used for a ScrollNode that doesn't have
    // main_thread_scrolling_reasons but is_composited is false.
    kHasBackgroundAttachmentFixedObjects = 1 << 2,
    // 1 << 4 is used by kPopupNoThreadedInput below.
    // Subpixel (LCD) text rendering requires blending glyphs with an opaque
    // background.
    kNotOpaqueForTextAndLCDText = 1 << 5,
    kPreferNonCompositedScrolling = 1 << 15,
    kBackgroundNeedsRepaintOnScroll = 1 << 16,

    // Main-thread hit-test reasons.
    // See InputHandler::ScrollStatus::main_thread_hit_test_reasons.
    kScrollbarScrolling = 1 << 7,
    kMainThreadScrollHitTestRegion = 1 << 8,
    kFailedHitTest = 1 << 9,
    // 1 << 10 is used by kNoScrollingLayer above.

    // The following reasons are neither repaint reasons nor hit-test reasons.
    // They don't go through InputHandler::ScrollBegin() or set in
    // InputHandler::ScrollStatus.

    // We need main thread Scrolling in a popup because it doesn't have a
    // threaded input handler. This flag is used in blink only, to prevent
    // composited scroll animation in a popup.
    // See blink::ScrollAnimator::SendAnimationToCompositor().
    kPopupNoThreadedInput = 1 << 4,

    // Scrolling can be handled on the compositor thread but it might be
    // blocked on the main thread waiting for non-passive event handlers to
    // process the wheel/touch events (i.e. were they preventDefaulted?).
    kWheelEventHandlerRegion = 1 << 13,
    kTouchEventHandlerRegion = 1 << 14,

    // 1 << 15 is used by kPreferNonCompositedScrolling above.
    // 1 << 16 is used by kBackgroundNeedsRepaintOnScroll above.

    // For blink::RecordScrollReasonsMetric() to know the number of used bits.
    kMainThreadScrollingReasonLast = 16,
  };

  static constexpr uint32_t kRepaintReasons =
      kHasBackgroundAttachmentFixedObjects | kNotOpaqueForTextAndLCDText |
      kPreferNonCompositedScrolling | kBackgroundNeedsRepaintOnScroll;
  static constexpr uint32_t kHitTestReasons =
      kScrollbarScrolling | kMainThreadScrollHitTestRegion | kFailedHitTest;

  static bool AreRepaintReasons(uint32_t reasons) {
    return (reasons & ~kRepaintReasons) == 0;
  }
  static bool AreHitTestReasons(uint32_t reasons) {
    return (reasons & ~kHitTestReasons) == 0;
  }

  static int BucketIndexForTesting(uint32_t reason);

  static std::string AsText(uint32_t reasons);
  static void AddToTracedValue(uint32_t reasons,
                               base::trace_event::TracedValue&);
};

}  // namespace cc

#endif  // CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_

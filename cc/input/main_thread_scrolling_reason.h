// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_
#define CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_

#include <stdint.h>

#include <iosfwd>
#include <memory>
#include <string>

#include "base/containers/enum_set.h"
#include "cc/cc_export.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace cc {

enum class MainThreadRepaintReason {
  kMinValue,
  // See InputHandler::ScrollStatus::main_thread_repaint_reasons.
  // They are set in ScrollNode::main_thread_scrolling_reasons, or
  // kNoScrollingLayer is used for a ScrollNode that doesn't have
  // main_thread_scrolling_reasons but is_composited is false.
  kHasBackgroundAttachmentFixedObjects = kMinValue,
  // Subpixel (LCD) text rendering requires blending glyphs with an opaque
  // background.
  kNotOpaqueForTextAndLCDText,
  kPreferNonCompositedScrolling,
  kBackgroundNeedsRepaintOnScroll,
  kMaxValue = kBackgroundNeedsRepaintOnScroll,
};

using MainThreadRepaintReasons = base::EnumSet<MainThreadRepaintReason>;

// See InputHandler::ScrollStatus::main_thread_hit_test_reasons.
enum class MainThreadHitTestReason {
  kMinValue,
  kScrollbarScrolling = kMinValue,
  kMainThreadScrollHitTestRegion,
  kFailedHitTest,
  kMaxValue = kFailedHitTest,
};

using MainThreadHitTestReasons = base::EnumSet<MainThreadHitTestReason>;

// The following reasons are neither repaint reasons nor hit-test reasons.
// They don't go through InputHandler::ScrollBegin() or set in
// InputHandler::ScrollStatus.
enum class MainThreadScrollingOtherReason {
  kMinValue,
  // We need main thread scrolling in a popup because it doesn't have a
  // threaded input handler. This flag is for metrics only, see
  // blink::WebPagePopupImpl::HandleGestureEvent.
  kPopupNoThreadedInput = kMinValue,

  // Scrolling can be handled on the compositor thread but it might be
  // blocked on the main thread waiting for non-passive event handlers to
  // process the wheel/touch events (i.e. were they preventDefaulted?).
  kWheelEventHandlerRegion,
  kTouchEventHandlerRegion,
  kMaxValue = kTouchEventHandlerRegion,
};

using MainThreadScrollingOtherReasons =
    base::EnumSet<MainThreadScrollingOtherReason>;

// Utility functions.
class CC_EXPORT MainThreadScrollingReason {
 public:
  MainThreadScrollingReason() = delete;

  static std::string AsText(MainThreadRepaintReasons);
  static std::string AsText(MainThreadHitTestReasons);
  static std::string AsText(MainThreadScrollingOtherReasons);
  static void AddToTracedValue(MainThreadRepaintReasons,
                               base::trace_event::TracedValue&);
  static void AddToTracedValue(MainThreadHitTestReasons,
                               base::trace_event::TracedValue&);
  static void AddToTracedValue(MainThreadScrollingOtherReasons,
                               base::trace_event::TracedValue&);
};

// These are declared here for use in gtest-based unit tests but are defined in
// the //cc:test_support target. Depend on that to use them in a unit test.
void PrintTo(MainThreadRepaintReasons, std::ostream*);
void PrintTo(MainThreadHitTestReasons, std::ostream*);
void PrintTo(MainThreadScrollingOtherReasons, std::ostream*);

}  // namespace cc

#endif  // CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_

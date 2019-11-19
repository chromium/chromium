// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/main_thread_scrolling_reason.h"

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/trace_event/traced_value.h"

namespace cc {

std::string MainThreadScrollingReason::AsText(uint32_t reasons) {
  base::trace_event::TracedValue traced_value;
  AddToTracedValue(reasons, traced_value);
  std::string result = traced_value.ToString();
  // Remove '{main_thread_scrolling_reasons:[', ']}', and any '"' chars.
  size_t array_start_pos = result.find('[');
  size_t array_end_pos = result.find(']');
  result =
      result.substr(array_start_pos + 1, array_end_pos - array_start_pos - 1);
  base::Erase(result, '\"');
  // Add spaces after all commas.
  base::ReplaceChars(result, ",", ", ", &result);
  return result;
}

void MainThreadScrollingReason::AddToTracedValue(
    uint32_t reasons,
    base::trace_event::TracedValue& traced_value) {
  traced_value.BeginArray("main_thread_scrolling_reasons");

  if (reasons & kHasBackgroundAttachmentFixedObjects)
    traced_value.AppendString("Has background-attachment:fixed");
  if (reasons & kHasNonLayerViewportConstrainedObjects)
    traced_value.AppendString("Has non-layer viewport-constrained objects");
  if (reasons & kThreadedScrollingDisabled)
    traced_value.AppendString("Threaded scrolling is disabled");
  if (reasons & kScrollbarScrolling)
    traced_value.AppendString("Scrollbar scrolling");
  if (reasons & kFrameOverlay)
    traced_value.AppendString("Frame overlay");
  if (reasons & kHandlingScrollFromMainThread)
    traced_value.AppendString("Handling scroll from main thread");
  if (reasons & kHasOpacityAndLCDText)
    traced_value.AppendString("Has opacity and LCD text");
  if (reasons & kHasTransformAndLCDText)
    traced_value.AppendString("Has transform and LCD text");
  if (reasons & kBackgroundNotOpaqueInRectAndLCDText)
    traced_value.AppendString("Background is not opaque in rect and LCD text");
  if (reasons & kHasClipRelatedProperty)
    traced_value.AppendString("Has clip related property");
  if (reasons & kHasBoxShadowFromNonRootLayer)
    traced_value.AppendString("Has box shadow from non-root layer");
  if (reasons & kIsNotStackingContextAndLCDText)
    traced_value.AppendString("Is not stacking context and LCD text");

  // Transient scrolling reasons.
  if (reasons & kNonFastScrollableRegion)
    traced_value.AppendString("Non fast scrollable region");
  if (reasons & kFailedHitTest)
    traced_value.AppendString("Failed hit test");
  if (reasons & kNoScrollingLayer)
    traced_value.AppendString("No scrolling layer");
  if (reasons & kNotScrollable)
    traced_value.AppendString("Not scrollable");
  if (reasons & kContinuingMainThreadScroll)
    traced_value.AppendString("Continuing main thread scroll");
  if (reasons & kNonInvertibleTransform)
    traced_value.AppendString("Non-invertible transform");
  if (reasons & kPageBasedScrolling)
    traced_value.AppendString("Page-based scrolling");
  if (reasons & kWheelEventHandlerRegion)
    traced_value.AppendString("Wheel event handler region");
  if (reasons & kTouchEventHandlerRegion)
    traced_value.AppendString("Touch event handler region");

  traced_value.EndArray();
}

}  // namespace cc

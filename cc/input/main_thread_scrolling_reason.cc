// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/main_thread_scrolling_reason.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/trace_event/traced_value.h"

namespace cc {

std::string MainThreadScrollingReason::AsText(uint32_t reasons) {
  base::trace_event::TracedValueJSON traced_value;
  AddToTracedValue(reasons, traced_value);
  std::string result = traced_value.ToJSON();
  // Remove '{main_thread_scrolling_reasons:[', ']}', and any '"' chars.
  size_t array_start_pos = result.find('[');
  size_t array_end_pos = result.find(']');
  result =
      result.substr(array_start_pos + 1, array_end_pos - array_start_pos - 1);
  std::erase(result, '\"');
  // Add spaces after all commas.
  base::ReplaceChars(result, ",", ", ", &result);
  return result;
}

void MainThreadScrollingReason::AddToTracedValue(
    uint32_t reasons,
    base::trace_event::TracedValue& traced_value) {
  traced_value.BeginArray("main_thread_scrolling_reasons");

#define ADD_REASON(reason, string)       \
  do                                     \
    if (reasons & reason) {              \
      traced_value.AppendString(string); \
      reasons &= ~reason;                \
    }                                    \
  while (false)

  ADD_REASON(kHasBackgroundAttachmentFixedObjects,
             "Has background-attachment:fixed");
  ADD_REASON(kNotOpaqueForTextAndLCDText, "Not opaque for text and LCD text");
  ADD_REASON(kPreferNonCompositedScrolling, "Prefer non-composited scrolling");
  ADD_REASON(kBackgroundNeedsRepaintOnScroll,
             "Background needs repaint on scroll");
  ADD_REASON(kScrollbarScrolling, "Scrollbar scrolling");
  ADD_REASON(kMainThreadScrollHitTestRegion,
             "Main thread scroll hit test region");
  ADD_REASON(kFailedHitTest, "Failed hit test");
  ADD_REASON(kPopupNoThreadedInput,
             "Popup scrolling (no threaded input handler)");
  ADD_REASON(kWheelEventHandlerRegion, "Wheel event handler region");
  ADD_REASON(kTouchEventHandlerRegion, "Touch event handler region");

#undef ADD_REASON

  DCHECK_EQ(reasons, kNotScrollingOnMain);
  traced_value.EndArray();
}

int MainThreadScrollingReason::BucketIndexForTesting(uint32_t reason) {
  // These two values are already bucket indices.
  DCHECK_NE(reason, kNotScrollingOnMain);
  DCHECK_NE(reason, kScrollingOnMainForAnyReason);

  int index = 0;
  while (reason >>= 1)
    ++index;
  DCHECK_NE(index, 0);
  return index;
}

}  // namespace cc

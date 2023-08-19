// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/main_thread_scrolling_reason.h"

#include "base/containers/cxx20_erase.h"
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
  if (reasons & kScrollbarScrolling)
    traced_value.AppendString("Scrollbar scrolling");
  if (reasons & kNotOpaqueForTextAndLCDText)
    traced_value.AppendString("Not opaque for text and LCD text");
  if (reasons & kCantPaintScrollingBackgroundAndLCDText)
    traced_value.AppendString("Can't paint scrolling background and LCD text");

  // Transient scrolling reasons.
  if (reasons & kNonFastScrollableRegion)
    traced_value.AppendString("Non fast scrollable region");
  if (reasons & kFailedHitTest)
    traced_value.AppendString("Failed hit test");
  if (reasons & kNoScrollingLayer)
    traced_value.AppendString("No scrolling layer");
  if (reasons & kWheelEventHandlerRegion)
    traced_value.AppendString("Wheel event handler region");
  if (reasons & kTouchEventHandlerRegion)
    traced_value.AppendString("Touch event handler region");

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

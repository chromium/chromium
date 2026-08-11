// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/main_thread_scrolling_reason.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/trace_event/traced_value.h"

namespace cc {

namespace {

template <typename Reasons>
std::string AsTextImpl(Reasons reasons) {
  base::trace_event::TracedValueJSON traced_value;
  MainThreadScrollingReason::AddToTracedValue(reasons, traced_value);
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

}  // namespace

std::string MainThreadScrollingReason::AsText(
    MainThreadRepaintReasons reasons) {
  return AsTextImpl(reasons);
}

std::string MainThreadScrollingReason::AsText(
    MainThreadHitTestReasons reasons) {
  return AsTextImpl(reasons);
}

std::string MainThreadScrollingReason::AsText(
    MainThreadScrollingOtherReasons reasons) {
  return AsTextImpl(reasons);
}

#define ADD_REASON(reason, string)       \
  do                                     \
    if (reasons.Has(reason)) {           \
      traced_value.AppendString(string); \
      reasons.Remove(reason);            \
    }                                    \
  while (false)

void MainThreadScrollingReason::AddToTracedValue(
    MainThreadRepaintReasons reasons,
    base::trace_event::TracedValue& traced_value) {
  traced_value.BeginArray("main_thread_scrolling_reasons");
  ADD_REASON(MainThreadRepaintReason::kHasBackgroundAttachmentFixedObjects,
             "Has background-attachment:fixed");
  ADD_REASON(MainThreadRepaintReason::kNotOpaqueForTextAndLCDText,
             "Not opaque for text and LCD text");
  ADD_REASON(MainThreadRepaintReason::kPreferNonCompositedScrolling,
             "Prefer non-composited scrolling");
  ADD_REASON(MainThreadRepaintReason::kBackgroundNeedsRepaintOnScroll,
             "Background needs repaint on scroll");
  DCHECK(reasons.empty());
  traced_value.EndArray();
}

void MainThreadScrollingReason::AddToTracedValue(
    MainThreadHitTestReasons reasons,
    base::trace_event::TracedValue& traced_value) {
  traced_value.BeginArray("main_thread_scrolling_reasons");
  ADD_REASON(MainThreadHitTestReason::kScrollbarScrolling,
             "Scrollbar scrolling");
  ADD_REASON(MainThreadHitTestReason::kMainThreadScrollHitTestRegion,
             "Main thread scroll hit test region");
  ADD_REASON(MainThreadHitTestReason::kFailedHitTest, "Failed hit test");
  DCHECK(reasons.empty());
  traced_value.EndArray();
}

void MainThreadScrollingReason::AddToTracedValue(
    MainThreadScrollingOtherReasons reasons,
    base::trace_event::TracedValue& traced_value) {
  traced_value.BeginArray("main_thread_scrolling_reasons");
  ADD_REASON(MainThreadScrollingOtherReason::kPopupNoThreadedInput,
             "Popup scrolling (no threaded input handler)");
  ADD_REASON(MainThreadScrollingOtherReason::kWheelEventHandlerRegion,
             "Wheel event handler region");
  ADD_REASON(MainThreadScrollingOtherReason::kTouchEventHandlerRegion,
             "Touch event handler region");
  DCHECK(reasons.empty());
  traced_value.EndArray();
}

}  // namespace cc

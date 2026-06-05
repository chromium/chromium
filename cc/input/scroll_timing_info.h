// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_TIMING_INFO_H_
#define CC_INPUT_SCROLL_TIMING_INFO_H_

#include <optional>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/paint/element_id.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {

// Per-scroll timing data for the Performance Scroll Timing API, sent from
// the compositor thread to the main thread on commit to build entries. On
// the compositor it also doubles as the in-flight tracking record: a
// `nullopt` `end_time` or empty `element_id` mean the scroll has not yet
// ended or latched a scroller.
struct CC_EXPORT ScrollTimingInfo {
  ElementId element_id;
  base::TimeTicks start_time;
  // Unset while the scroll is still in flight; set when the gesture ends.
  std::optional<base::TimeTicks> end_time;
  std::optional<ui::ScrollInputType> input_type;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLL_TIMING_INFO_H_

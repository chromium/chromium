// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_TIMING_INFO_H_
#define CC_METRICS_SCROLL_TIMING_INFO_H_

#include <ostream>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/paint/element_id.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {

// A finalized scroll segment, ready for delivery to the main thread.
//
// `start_time` is the original GestureScrollBegin hardware timestamp of the
// gesture; `end_time` is the presentation timestamp of the last frame which
// carried movement attributable to the segment.
struct CC_EXPORT ScrollTimingInfo {
  base::TimeTicks start_time;
  base::TimeTicks end_time;
  ui::ScrollInputType input_type;
  ElementId element_id;

  bool operator==(const ScrollTimingInfo&) const = default;
};

inline std::ostream& operator<<(std::ostream& os,
                                const ScrollTimingInfo& info) {
  return os << "ScrollTimingInfo{start_time: " << info.start_time
            << ", end_time: " << info.end_time
            << ", input_type: " << static_cast<int>(info.input_type)
            << ", element_id: " << info.element_id << "}";
}

}  // namespace cc

#endif  // CC_METRICS_SCROLL_TIMING_INFO_H_

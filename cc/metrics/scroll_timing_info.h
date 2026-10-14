// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_TIMING_INFO_H_
#define CC_METRICS_SCROLL_TIMING_INFO_H_

#include <cstdint>
#include <ostream>
#include <type_traits>

#include "base/memory/shared_memory_safety_checker.h"
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
  uint32_t reserved = 0;  // Explicit padding; must stay zero.
  ElementId element_id;

  bool operator==(const ScrollTimingInfo&) const = default;
};

// ScrollTimingBuffer stores ScrollTimingInfo in shared memory. For structs,
// AllowedOverSharedMemory only requires trivial copyability, which still
// allows pointer members, so check each member too. Add new members here.
static_assert(std::is_trivially_copyable_v<ScrollTimingInfo>);
static_assert(std::has_unique_object_representations_v<ScrollTimingInfo>,
              "ScrollTimingInfo is copied over shared memory; no padding "
              "allowed");
static_assert(base::subtle::AllowedOverSharedMemory<
              decltype(ScrollTimingInfo::start_time)>);
static_assert(base::subtle::AllowedOverSharedMemory<
              decltype(ScrollTimingInfo::end_time)>);
static_assert(base::subtle::AllowedOverSharedMemory<
              decltype(ScrollTimingInfo::input_type)>);
static_assert(base::subtle::AllowedOverSharedMemory<
              decltype(ScrollTimingInfo::reserved)>);
static_assert(base::subtle::AllowedOverSharedMemory<
              decltype(ScrollTimingInfo::element_id)>);

inline std::ostream& operator<<(std::ostream& os,
                                const ScrollTimingInfo& info) {
  return os << "ScrollTimingInfo{start_time: " << info.start_time
            << ", end_time: " << info.end_time
            << ", input_type: " << static_cast<int>(info.input_type)
            << ", element_id: " << info.element_id << "}";
}

}  // namespace cc

#endif  // CC_METRICS_SCROLL_TIMING_INFO_H_

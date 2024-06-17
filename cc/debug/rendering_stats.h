// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_RENDERING_STATS_H_
#define CC_DEBUG_RENDERING_STATS_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "cc/debug/debug_export.h"

namespace cc {

struct CC_DEBUG_EXPORT RenderingStats {
  // Stores a sequence of TimeDelta objects.
  class CC_DEBUG_EXPORT TimeDeltaList {
   public:
    TimeDeltaList();
    TimeDeltaList(const TimeDeltaList& other);
    ~TimeDeltaList();

    void Append(base::TimeDelta value);
    void AddToTracedValue(const char* name,
                          base::trace_event::TracedValue* list_value) const;

    void Add(const TimeDeltaList& other);

    base::TimeDelta GetLastTimeDelta() const;

   private:
    std::vector<base::TimeDelta> values;
  };

  RenderingStats();
  RenderingStats(const RenderingStats& other);
  ~RenderingStats();

  int64_t frame_count = 0;
  int64_t visible_content_area = 0;
  int64_t approximated_visible_content_area = 0;
  int64_t checkerboarded_visible_content_area = 0;
  int64_t checkerboarded_needs_record_content_area = 0;
  int64_t checkerboarded_needs_raster_content_area = 0;

  TimeDeltaList draw_duration;
  TimeDeltaList draw_duration_estimate;
  TimeDeltaList begin_main_frame_to_commit_duration;
  TimeDeltaList commit_to_activate_duration;
  TimeDeltaList commit_to_activate_duration_estimate;

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsTraceableData()
      const;
};

}  // namespace cc

#endif  // CC_DEBUG_RENDERING_STATS_H_

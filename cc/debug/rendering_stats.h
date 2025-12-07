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
  RenderingStats();
  RenderingStats(const RenderingStats& other) = delete;
  RenderingStats(RenderingStats&& other);
  RenderingStats& operator=(const RenderingStats& other) = delete;
  RenderingStats& operator=(RenderingStats&& other);
  ~RenderingStats();

  int64_t visible_content_area = 0;
  int64_t approximated_visible_content_area = 0;

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsTraceableData()
      const;
};

}  // namespace cc

#endif  // CC_DEBUG_RENDERING_STATS_H_

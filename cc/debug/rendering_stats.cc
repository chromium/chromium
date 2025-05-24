// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/rendering_stats.h"

#include <utility>

namespace cc {

RenderingStats::RenderingStats() = default;

RenderingStats::RenderingStats(RenderingStats&& other) = default;

RenderingStats& RenderingStats::operator=(RenderingStats&& other) = default;

RenderingStats::~RenderingStats() = default;

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
RenderingStats::AsTraceableData() const {
  std::unique_ptr<base::trace_event::TracedValue> record_data(
      new base::trace_event::TracedValue());
  record_data->SetInteger("visible_content_area", visible_content_area);
  record_data->SetInteger("approximated_visible_content_area",
                          approximated_visible_content_area);
  return std::move(record_data);
}

}  // namespace cc

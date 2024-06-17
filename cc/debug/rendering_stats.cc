// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/rendering_stats.h"

#include <utility>

namespace cc {

RenderingStats::TimeDeltaList::TimeDeltaList() = default;

RenderingStats::TimeDeltaList::TimeDeltaList(const TimeDeltaList& other) =
    default;

RenderingStats::TimeDeltaList::~TimeDeltaList() = default;

void RenderingStats::TimeDeltaList::Append(base::TimeDelta value) {
  values.push_back(value);
}

void RenderingStats::TimeDeltaList::AddToTracedValue(
    const char* name,
    base::trace_event::TracedValue* list_value) const {
  list_value->BeginArray(name);
  for (const auto& value : values) {
    list_value->AppendDouble(value.InMillisecondsF());
  }
  list_value->EndArray();
}

void RenderingStats::TimeDeltaList::Add(const TimeDeltaList& other) {
  values.insert(values.end(), other.values.begin(), other.values.end());
}

base::TimeDelta RenderingStats::TimeDeltaList::GetLastTimeDelta() const {
  return values.empty() ? base::TimeDelta() : values.back();
}

RenderingStats::RenderingStats() = default;

RenderingStats::RenderingStats(const RenderingStats& other) = default;

RenderingStats::~RenderingStats() = default;

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
RenderingStats::AsTraceableData() const {
  std::unique_ptr<base::trace_event::TracedValue> record_data(
      new base::trace_event::TracedValue());
  record_data->SetInteger("frame_count", frame_count);
  record_data->SetInteger("visible_content_area", visible_content_area);
  record_data->SetInteger("approximated_visible_content_area",
                          approximated_visible_content_area);
  record_data->SetInteger("checkerboarded_visible_content_area",
                          checkerboarded_visible_content_area);
  record_data->SetInteger("checkerboarded_needs_record_content_area",
                          checkerboarded_needs_record_content_area);
  record_data->SetInteger("checkerboarded_needs_raster_content_area",
                          checkerboarded_needs_raster_content_area);
  draw_duration.AddToTracedValue("draw_duration_ms", record_data.get());

  draw_duration_estimate.AddToTracedValue("draw_duration_estimate_ms",
                                          record_data.get());

  begin_main_frame_to_commit_duration.AddToTracedValue(
      "begin_main_frame_to_commit_duration_ms", record_data.get());

  commit_to_activate_duration.AddToTracedValue("commit_to_activate_duration_ms",
                                               record_data.get());

  commit_to_activate_duration_estimate.AddToTracedValue(
      "commit_to_activate_duration_estimate_ms", record_data.get());
  return std::move(record_data);
}

}  // namespace cc

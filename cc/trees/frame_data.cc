// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/frame_data.h"

#include <string>
#include <unordered_map>

#include "base/trace_event/traced_value.h"
#include "base/trace_event/typed_macros.h"
#include "cc/base/histograms.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_latency_info.pbzero.h"

namespace cc {
FrameData::FrameData() = default;
FrameData::~FrameData() = default;

void FrameData::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetBoolean("has_no_damage", has_no_damage);
  const char* client_name = GetClientNameForMetrics();
  bool is_renderer =
      client_name && UNSAFE_TODO(strcmp(client_name, "Renderer")) == 0;

  bool quads_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
                                     &quads_enabled);

  // Quad data can be quite large, so only dump render passes from the renderer
  // if viz.quads tracing category is enabled.
  if (is_renderer && quads_enabled) {
    std::unordered_map<viz::ResourceId, size_t> resource_id_to_index_map;
    value->BeginArray("render_passes");
    for (const auto& render_pass : render_passes) {
      value->BeginDictionary();
      render_pass->AsValueInto(value, resource_id_to_index_map);
      value->EndDictionary();
    }
    value->EndArray();
  }
}

std::string FrameData::ToString() const {
  base::trace_event::TracedValueJSON value;
  AsValueInto(&value);
  return value.ToFormattedJSON();
}

void FrameData::set_trees_in_viz_timestamps(
    const viz::TreesInVizTiming& timing_details) {
  trees_in_viz_timing_details = timing_details;
}

}  // namespace cc

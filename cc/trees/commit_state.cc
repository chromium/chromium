// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/trees/commit_state.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"

namespace cc {

CommitState::CommitState() = default;
CommitState::~CommitState() = default;
CommitState::CommitState(const CommitState& prev)
    : surface_ranges(prev.surface_ranges),
      visual_properties_update_duration(prev.visual_properties_update_duration),
      have_scroll_event_handlers(prev.have_scroll_event_handlers),
      is_external_pinch_gesture_active(prev.is_external_pinch_gesture_active),
      is_viewport_mobile_optimized(prev.is_viewport_mobile_optimized),
      may_throttle_if_undrawn_frames(prev.may_throttle_if_undrawn_frames),
      prefers_reduced_motion(prev.prefers_reduced_motion),
      browser_controls_params(prev.browser_controls_params),
      bottom_controls_shown_ratio(prev.bottom_controls_shown_ratio),
      device_scale_factor(prev.device_scale_factor),
      external_page_scale_factor(prev.external_page_scale_factor),
      max_page_scale_factor(prev.max_page_scale_factor),
      min_page_scale_factor(prev.min_page_scale_factor),
      page_scale_factor(prev.page_scale_factor),
      painted_device_scale_factor(prev.painted_device_scale_factor),
      top_controls_shown_ratio(prev.top_controls_shown_ratio),
      display_color_spaces(prev.display_color_spaces),
      display_transform_hint(prev.display_transform_hint),
      device_viewport_rect(prev.device_viewport_rect),
      visual_device_viewport_size(prev.visual_device_viewport_size),
      elastic_overscroll(prev.elastic_overscroll),
      hud_layer_id(prev.hud_layer_id),
      source_frame_number(prev.source_frame_number),
      selection(prev.selection),
      debug_state(prev.debug_state),
      overscroll_behavior(prev.overscroll_behavior),
      background_color(prev.background_color),
      viewport_property_ids(prev.viewport_property_ids),
      local_surface_id_from_parent(prev.local_surface_id_from_parent),
      primary_main_frame_item_sequence_number(
          prev.primary_main_frame_item_sequence_number) {
  memcpy(event_listener_properties, prev.event_listener_properties,
         sizeof(event_listener_properties));
}

base::flat_set<viz::SurfaceRange> CommitState::SurfaceRanges() const {
  base::flat_set<viz::SurfaceRange> ranges;
  for (auto& map_entry : surface_ranges)
    ranges.insert(map_entry.first);
  return ranges;
}

EventListenerProperties CommitState::GetEventListenerProperties(
    EventListenerClass listener_class) const {
  DCHECK(listener_class >= EventListenerClass::kPointerRawUpdate);
  DCHECK(listener_class <= EventListenerClass::kTouchEndOrCancel);
  return event_listener_properties[static_cast<size_t>(listener_class)];
}

ThreadUnsafeCommitState::ThreadUnsafeCommitState(
    MutatorHost* mh,
    const ProtectedSequenceSynchronizer& synchronizer)
    : mutator_host(mh), property_trees(synchronizer) {}

ThreadUnsafeCommitState::~ThreadUnsafeCommitState() = default;

}  // namespace cc

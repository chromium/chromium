// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_state_data.h"
#include "cc/trees/scroll_node.h"

namespace cc {

ScrollStateData::ScrollStateData()
    : delta_x(0),
      delta_y(0),
      delta_x_hint(0),
      delta_y_hint(0),
      position_x(0),
      position_y(0),
      is_beginning(false),
      is_in_inertial_phase(false),
      is_ending(false),
      from_user_input(false),
      delta_consumed_for_scroll_sequence(false),
      is_direct_manipulation(false),
      is_scrollbar_interaction(false),
      delta_granularity(ui::ScrollGranularity::kScrollByPrecisePixel),
      caused_scroll_x(false),
      caused_scroll_y(false),
      is_scroll_chain_cut(false) {}

ScrollStateData::ScrollStateData(const ScrollStateData&) = default;

ScrollStateData& ScrollStateData::operator=(const ScrollStateData&) = default;

ElementId ScrollStateData::current_native_scrolling_element() const {
  return current_native_scrolling_element_;
}

void ScrollStateData::set_current_native_scrolling_element(
    ElementId element_id) {
  current_native_scrolling_element_ = element_id;
}

}  // namespace cc

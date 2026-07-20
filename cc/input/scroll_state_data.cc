// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_state_data.h"
#include "cc/trees/scroll_node.h"

namespace cc {

ScrollStateData::ScrollStateData() = default;

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

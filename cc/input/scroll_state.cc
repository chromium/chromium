// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_state.h"

#include <utility>

#include "cc/trees/scroll_node.h"

namespace cc {

ScrollState::ScrollState(ScrollStateData data) : data_(data) {}

ScrollState::ScrollState(const ScrollState& other) = default;

ScrollState::~ScrollState() = default;

void ScrollState::ConsumeDelta(double x, double y) {
  data_.delta_x -= x;
  data_.delta_y -= y;

  if (x || y)
    data_.delta_consumed_for_scroll_sequence = true;
}

gfx::Vector2dF ScrollState::DeltaOrHint() const {
  if (is_beginning())
    return gfx::Vector2dF(delta_x_hint(), delta_y_hint());

  return gfx::Vector2dF(delta_x(), delta_y());
}

}  // namespace cc

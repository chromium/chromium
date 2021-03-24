// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/marker/marker_controller_test_api.h"

#include "ash/fast_ink/fast_ink_points.h"
#include "ash/highlighter/highlighter_view.h"
#include "ash/marker/marker_controller.h"

namespace ash {

MarkerControllerTestApi::MarkerControllerTestApi(MarkerController* instance)
    : instance_(instance) {}

MarkerControllerTestApi::~MarkerControllerTestApi() = default;

bool MarkerControllerTestApi::IsShowingMarker() const {
  return !!instance_->marker_view_widget_;
}

const fast_ink::FastInkPoints& MarkerControllerTestApi::points() const {
  return instance_->GetMarkerView()->points_;
}

}  // namespace ash

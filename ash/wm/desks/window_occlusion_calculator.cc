// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/window_occlusion_calculator.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/wm/desks/legacy_window_occlusion_calculator.h"
#include "ash/wm/desks/new_window_occlusion_calculator.h"

namespace ash {

// static
std::unique_ptr<WindowOcclusionCalculator> WindowOcclusionCalculator::Create() {
  if (features::IsNewWindowOcclusionCalculatorEnabled()) {
    return std::make_unique<NewWindowOcclusionCalculator>();
  }
  return std::make_unique<legacy::WindowOcclusionCalculator>();
}

}  // namespace ash

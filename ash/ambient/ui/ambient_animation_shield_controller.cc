// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_shield_controller.h"

#include "base/check.h"
#include "base/logging.h"
#include "ui/views/view.h"

namespace ash {

AmbientAnimationShieldController::AmbientAnimationShieldController(
    std::unique_ptr<views::View> shield_view,
    views::View* parent_view)
    : shield_view_(std::move(shield_view)), parent_view_(parent_view) {
  DCHECK(shield_view_);
  DCHECK(parent_view_);
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  color_provider_observer_.Observe(dark_light_mode_controller);
  // Call OnColorModeChanged() directly to capture the initial dark-mode value.
  OnColorModeChanged(dark_light_mode_controller->IsDarkModeEnabled());
}

AmbientAnimationShieldController::~AmbientAnimationShieldController() = default;

void AmbientAnimationShieldController::OnColorModeChanged(
    bool dark_mode_enabled) {
  bool shield_is_active = parent_view_->Contains(shield_view_.get());
  if (dark_mode_enabled && !shield_is_active) {
    DVLOG(4) << "Adding dark mode shield";
    parent_view_->AddChildView(shield_view_.get());
  } else if (!dark_mode_enabled && shield_is_active) {
    DVLOG(4) << "Removing dark mode shield";
    parent_view_->RemoveChildView(shield_view_.get());
  }
}

}  // namespace ash

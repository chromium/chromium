// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/unified_media_controls_container.h"

#include "ash/system/tray/tray_constants.h"
#include "ui/views/border.h"

namespace ash {

namespace {
constexpr int kContainerHeight = 72;
constexpr gfx::Insets kContainerInsets = gfx::Insets(0, 16, 16, 16);
}  // namespace

UnifiedMediaControlsContainer::UnifiedMediaControlsContainer()
    : expanded_amount_(0.0f), should_show_media_controls_(false) {
  SetBorder(views::CreateEmptyBorder(kContainerInsets));
}

void UnifiedMediaControlsContainer::SetShouldShowMediaControls(
    bool should_show) {
  should_show_media_controls_ = should_show;
  SetVisible(expanded_amount_ > 0 && should_show_media_controls_);
  InvalidateLayout();
}

void UnifiedMediaControlsContainer::SetExpandedAmount(double expanded_amount) {
  SetVisible(expanded_amount > 0 && should_show_media_controls_);
  expanded_amount_ = expanded_amount;
  for (auto* child : children())
    child->layer()->SetOpacity(expanded_amount);
  InvalidateLayout();
}

int UnifiedMediaControlsContainer::GetExpandedHeight() const {
  return should_show_media_controls_ ? kContainerHeight : 0;
}

void UnifiedMediaControlsContainer::Layout() {
  for (auto* child : children())
    child->SetBoundsRect(GetContentsBounds());
  views::View::Layout();
}

gfx::Size UnifiedMediaControlsContainer::CalculatePreferredSize() const {
  return gfx::Size(kTrayMenuWidth, GetExpandedHeight() * expanded_amount_);
}

}  // namespace ash

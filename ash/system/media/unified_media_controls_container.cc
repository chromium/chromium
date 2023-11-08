// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/unified_media_controls_container.h"

#include "ash/system/tray/tray_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/border.h"

namespace ash {

namespace {
constexpr int kContainerHeight = 80;
constexpr auto kContainerInsets = gfx::Insets::TLBR(8, 16, 16, 16);
}  // namespace

UnifiedMediaControlsContainer::UnifiedMediaControlsContainer()
    : expanded_amount_(0.0f), should_show_media_controls_(false) {
  SetBorder(views::CreateEmptyBorder(kContainerInsets));
}

void UnifiedMediaControlsContainer::SetShouldShowMediaControls(
    bool should_show) {
  should_show_media_controls_ = should_show;
}

bool UnifiedMediaControlsContainer::MaybeShowMediaControls() {
  if (expanded_amount_ == 0 || !should_show_media_controls_ || GetVisible())
    return false;

  SetVisible(true);
  InvalidateLayout();
  return true;
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

BEGIN_METADATA(UnifiedMediaControlsContainer)
END_METADATA

}  // namespace ash

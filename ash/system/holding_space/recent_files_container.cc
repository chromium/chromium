// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/recent_files_container.h"

#include <memory>

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/system/holding_space/downloads_section.h"
#include "ash/system/holding_space/screen_captures_section.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

RecentFilesContainer::RecentFilesContainer(
    HoldingSpaceItemViewDelegate* delegate) {
  SetID(kHoldingSpaceRecentFilesContainerId);
  SetVisible(false);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kHoldingSpaceContainerPadding,
      kHoldingSpaceContainerChildSpacing));

  // Sections.
  sections_.push_back(
      AddChildView(std::make_unique<ScreenCapturesSection>(delegate)));
  sections_.push_back(
      AddChildView(std::make_unique<DownloadsSection>(delegate)));
}

RecentFilesContainer::~RecentFilesContainer() = default;

void RecentFilesContainer::Init() {
  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
  if (!model)
    return;

  for (HoldingSpaceItemViewsContainer* section : sections_)
    section->OnHoldingSpaceModelAttached(model);
}

void RecentFilesContainer::Reset() {
  for (HoldingSpaceItemViewsContainer* section : sections_)
    section->Reset();
}

void RecentFilesContainer::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void RecentFilesContainer::ChildVisibilityChanged(views::View* child) {
  // The recent files container should be visible iff it has visible children.
  bool visible = false;
  for (const views::View* c : children())
    visible |= c->GetVisible();

  if (visible != GetVisible())
    SetVisible(visible);

  views::View::ChildVisibilityChanged(child);
}

}  // namespace ash

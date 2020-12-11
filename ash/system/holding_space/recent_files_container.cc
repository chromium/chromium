// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/recent_files_container.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/system/holding_space/downloads_section.h"
#include "ash/system/holding_space/screen_captures_section.h"

namespace ash {

RecentFilesContainer::RecentFilesContainer(
    HoldingSpaceItemViewDelegate* delegate)
    : HoldingSpaceTrayChildBubble(delegate) {
  SetID(kHoldingSpaceRecentFilesContainerId);
}

RecentFilesContainer::~RecentFilesContainer() = default;

const char* RecentFilesContainer::GetClassName() const {
  return "RecentFilesContainer";
}

std::vector<std::unique_ptr<HoldingSpaceItemViewsContainer>>
RecentFilesContainer::CreateSections() {
  std::vector<std::unique_ptr<HoldingSpaceItemViewsContainer>> sections;
  sections.push_back(std::make_unique<ScreenCapturesSection>(delegate()));
  sections.push_back(std::make_unique<DownloadsSection>(delegate()));
  return sections;
}

}  // namespace ash

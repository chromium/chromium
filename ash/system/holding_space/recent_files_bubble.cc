// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/recent_files_bubble.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/system/holding_space/downloads_section.h"
#include "ash/system/holding_space/screen_captures_section.h"

namespace ash {

RecentFilesBubble::RecentFilesBubble(HoldingSpaceViewDelegate* delegate)
    : HoldingSpaceTrayChildBubble(delegate) {
  SetID(kHoldingSpaceRecentFilesBubbleId);
}

RecentFilesBubble::~RecentFilesBubble() = default;

const char* RecentFilesBubble::GetClassName() const {
  return "RecentFilesBubble";
}

std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>>
RecentFilesBubble::CreateSections() {
  std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>> sections;
  sections.push_back(std::make_unique<ScreenCapturesSection>(delegate()));
  sections.push_back(std::make_unique<DownloadsSection>(delegate()));
  return sections;
}

}  // namespace ash

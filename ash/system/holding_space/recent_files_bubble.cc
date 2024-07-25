// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/recent_files_bubble.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/system/holding_space/downloads_section.h"
#include "ash/system/holding_space/holding_space_ui.h"
#include "ash/system/holding_space/screen_captures_section.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

RecentFilesBubble::RecentFilesBubble(HoldingSpaceViewDelegate* delegate)
    : HoldingSpaceTrayChildBubble(delegate) {
  SetID(kHoldingSpaceRecentFilesBubbleId);
}

RecentFilesBubble::~RecentFilesBubble() = default;

std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>>
RecentFilesBubble::CreateSections() {
  std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>> sections;
  sections.push_back(std::make_unique<ScreenCapturesSection>(delegate()));
  sections.push_back(std::make_unique<DownloadsSection>(delegate()));
  return sections;
}

BEGIN_METADATA(RecentFilesBubble)
END_METADATA

}  // namespace ash

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/pinned_files_container.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/system/holding_space/pinned_files_section.h"

namespace ash {

PinnedFilesContainer::PinnedFilesContainer(
    HoldingSpaceItemViewDelegate* delegate)
    : HoldingSpaceTrayChildBubble(delegate) {
  SetID(kHoldingSpacePinnedFilesContainerId);
}

PinnedFilesContainer::~PinnedFilesContainer() = default;

const char* PinnedFilesContainer::GetClassName() const {
  return "PinnedFilesContainer";
}

std::vector<std::unique_ptr<HoldingSpaceItemViewsContainer>>
PinnedFilesContainer::CreateSections() {
  std::vector<std::unique_ptr<HoldingSpaceItemViewsContainer>> sections;
  sections.push_back(std::make_unique<PinnedFilesSection>(delegate()));
  return sections;
}

}  // namespace ash

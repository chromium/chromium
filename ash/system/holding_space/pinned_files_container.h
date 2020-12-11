// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_CONTAINER_H_
#define ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_CONTAINER_H_

#include <memory>
#include <vector>

#include "ash/system/holding_space/holding_space_tray_child_bubble.h"

namespace ash {

// TODO(dmblack): Rename to `PinnedFilesBubble`.
// Child bubble of `HoldingSpaceTrayBubble` for pinned files.
class PinnedFilesContainer : public HoldingSpaceTrayChildBubble {
 public:
  explicit PinnedFilesContainer(HoldingSpaceItemViewDelegate* delegate);
  PinnedFilesContainer(const PinnedFilesContainer& other) = delete;
  PinnedFilesContainer& operator=(const PinnedFilesContainer& other) = delete;
  ~PinnedFilesContainer() override;

  // HoldingSpaceTrayChildBubble:
  const char* GetClassName() const override;
  std::vector<std::unique_ptr<HoldingSpaceItemViewsContainer>> CreateSections()
      override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_CONTAINER_H_

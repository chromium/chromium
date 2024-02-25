// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_BUBBLE_H_
#define ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_BUBBLE_H_

#include <memory>
#include <vector>

#include "ash/system/holding_space/holding_space_tray_child_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// Child bubble of `HoldingSpaceTrayBubble` for pinned files.
class PinnedFilesBubble : public HoldingSpaceTrayChildBubble {
  METADATA_HEADER(PinnedFilesBubble, HoldingSpaceTrayChildBubble)

 public:
  explicit PinnedFilesBubble(HoldingSpaceViewDelegate* delegate);
  PinnedFilesBubble(const PinnedFilesBubble& other) = delete;
  PinnedFilesBubble& operator=(const PinnedFilesBubble& other) = delete;
  ~PinnedFilesBubble() override;

  // HoldingSpaceTrayChildBubble:
  std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>> CreateSections()
      override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_BUBBLE_H_

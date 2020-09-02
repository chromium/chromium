// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_CONTAINER_H_
#define ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_CONTAINER_H_

#include "ui/views/view.h"

namespace ash {

class HoldingSpaceItemChipsContainer;

// Container for pinned files that the user adds to the holding space bubble.
class PinnedFilesContainer : public views::View {
 public:
  PinnedFilesContainer();
  PinnedFilesContainer(const PinnedFilesContainer& other) = delete;
  PinnedFilesContainer& operator=(const PinnedFilesContainer& other) = delete;
  ~PinnedFilesContainer() override;

  // views::View:
  const char* GetClassName() const override;

 private:
  HoldingSpaceItemChipsContainer* item_chips_container_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_CONTAINER_H_

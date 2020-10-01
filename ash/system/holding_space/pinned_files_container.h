// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_CONTAINER_H_
#define ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_CONTAINER_H_

#include <map>

#include "ash/system/holding_space/holding_space_item_views_container.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class HoldingSpaceItemChipsContainer;
class HoldingSpaceItemViewDelegate;

// Container for pinned files that the user adds to the holding space bubble.
class PinnedFilesContainer : public HoldingSpaceItemViewsContainer {
 public:
  explicit PinnedFilesContainer(HoldingSpaceItemViewDelegate* delegate);
  PinnedFilesContainer(const PinnedFilesContainer& other) = delete;
  PinnedFilesContainer& operator=(const PinnedFilesContainer& other) = delete;
  ~PinnedFilesContainer() override;

  // HoldingSpaceItemViewsContainer:
  void ViewHierarchyChanged(const views::ViewHierarchyChangedDetails&) override;
  void AddHoldingSpaceItemView(const HoldingSpaceItem* item) override;
  void RemoveAllHoldingSpaceItemViews() override;
  void RemoveHoldingSpaceItemView(const HoldingSpaceItem* item) override;

 private:
  HoldingSpaceItemViewDelegate* const delegate_;

  views::Label* empty_prompt_label_ = nullptr;
  HoldingSpaceItemChipsContainer* item_chips_container_ = nullptr;

  std::map<std::string, views::View*> views_by_item_id_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_CONTAINER_H_

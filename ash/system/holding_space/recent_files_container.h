// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_CONTAINER_H_
#define ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_CONTAINER_H_

#include <map>

#include "ash/system/holding_space/holding_space_item_views_container.h"

namespace ash {

class HoldingSpaceItemChipsContainer;
class HoldingSpaceItemViewDelegate;

// Container for the recent files (Screenshots, downloads etc).
class RecentFilesContainer : public HoldingSpaceItemViewsContainer {
 public:
  explicit RecentFilesContainer(HoldingSpaceItemViewDelegate* delegate);
  RecentFilesContainer(const RecentFilesContainer& other) = delete;
  RecentFilesContainer& operator=(const RecentFilesContainer& other) = delete;
  ~RecentFilesContainer() override;

  // HoldingSpaceItemViewsContainer:
  void AddHoldingSpaceItemView(const HoldingSpaceItem* item) override;
  void RemoveAllHoldingSpaceItemViews() override;
  void RemoveHoldingSpaceItemView(const HoldingSpaceItem* item) override;

 private:
  HoldingSpaceItemViewDelegate* const delegate_;
  views::View* screenshots_container_ = nullptr;
  HoldingSpaceItemChipsContainer* recent_downloads_container_ = nullptr;

  std::map<std::string, views::View*> views_by_item_id_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_CONTAINER_H_

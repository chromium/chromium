// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_CONTAINER_H_
#define ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_CONTAINER_H_

#include <vector>

#include "ui/views/view.h"

namespace ash {

class HoldingSpaceItemViewDelegate;
class HoldingSpaceItemViewsContainer;

// Container for the recent files downloads and screen captures sections.
class RecentFilesContainer : public views::View {
 public:
  explicit RecentFilesContainer(HoldingSpaceItemViewDelegate* delegate);
  RecentFilesContainer(const RecentFilesContainer& other) = delete;
  RecentFilesContainer& operator=(const RecentFilesContainer& other) = delete;
  ~RecentFilesContainer() override;

  // Initializes the container.
  void Init();

  // Resets the container. Called when the tray bubble starts closing to
  // stop observing the holding space controller/model to ensure that no new
  // items are created while the bubble widget is being asynchronously closed.
  void Reset();

 private:
  // HoldingSpaceItemViewsContainer:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // Views owned by view hierarchy.
  std::vector<HoldingSpaceItemViewsContainer*> sections_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_CONTAINER_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_DOWNLOADS_SECTION_H_
#define ASH_SYSTEM_HOLDING_SPACE_DOWNLOADS_SECTION_H_

#include <map>

#include "ash/system/holding_space/holding_space_item_views_container.h"

namespace ash {

class HoldingSpaceItemChipsContainer;
class HoldingSpaceItemView;

// Section for downloads in the `RecentFilesContainer`.
class DownloadsSection : public HoldingSpaceItemViewsContainer {
 public:
  explicit DownloadsSection(HoldingSpaceItemViewDelegate* delegate);
  DownloadsSection(const DownloadsSection& other) = delete;
  DownloadsSection& operator=(const DownloadsSection& other) = delete;
  ~DownloadsSection() override;

  // HoldingSpaceItemViewsContainer:
  void ChildVisibilityChanged(views::View* child) override;
  void ViewHierarchyChanged(const views::ViewHierarchyChangedDetails&) override;
  bool ContainsHoldingSpaceItemView(const HoldingSpaceItem* item) override;
  bool ContainsHoldingSpaceItemViews() override;
  bool WillAddHoldingSpaceItemView(const HoldingSpaceItem* item) override;
  void AddHoldingSpaceItemView(const HoldingSpaceItem* item) override;
  void RemoveAllHoldingSpaceItemViews() override;
  void AnimateIn(ui::LayerAnimationObserver* observer) override;
  void AnimateOut(ui::LayerAnimationObserver* observer) override;

 private:
  // Owned by view hierarchy.
  HoldingSpaceItemChipsContainer* container_ = nullptr;
  views::View* header_ = nullptr;

  std::map<std::string, HoldingSpaceItemView*> views_by_item_id_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_DOWNLOADS_SECTION_H_

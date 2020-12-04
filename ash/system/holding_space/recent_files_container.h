// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_CONTAINER_H_
#define ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_CONTAINER_H_

#include <map>

#include "ash/system/holding_space/holding_space_item_views_container.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class HoldingSpaceItemChipsContainer;

// Container for the recent files (e.g. screen captures, downloads, etc).
class RecentFilesContainer : public HoldingSpaceItemViewsContainer {
 public:
  explicit RecentFilesContainer(HoldingSpaceItemViewDelegate* delegate);
  RecentFilesContainer(const RecentFilesContainer& other) = delete;
  RecentFilesContainer& operator=(const RecentFilesContainer& other) = delete;
  ~RecentFilesContainer() override;

  // HoldingSpaceItemViewsContainer:
  void ChildVisibilityChanged(views::View* child) override;
  void ViewHierarchyChanged(const views::ViewHierarchyChangedDetails&) override;
  bool ContainsHoldingSpaceItemView(const HoldingSpaceItem* item) override;
  bool ContainsHoldingSpaceItemViews() override;
  bool WillAddHoldingSpaceItemView(const HoldingSpaceItem* item) override;
  void AddHoldingSpaceItemView(const HoldingSpaceItem* item) override;
  void RemoveAllHoldingSpaceItemViews() override;
  void AnimateIn(ui::ImplicitAnimationObserver* observer) override;
  void AnimateOut(ui::ImplicitAnimationObserver* observer) override;

 private:
  void AddHoldingSpaceScreenCaptureView(const HoldingSpaceItem* item);
  void AddHoldingSpaceDownloadView(const HoldingSpaceItem* item);
  void OnScreenCapturesContainerViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details);
  void OnDownloadsContainerViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details);

  // Owned by view hierarchy.
  views::View* screen_captures_container_ = nullptr;
  views::Label* screen_captures_label_ = nullptr;
  HoldingSpaceItemChipsContainer* downloads_container_ = nullptr;
  views::View* downloads_header_ = nullptr;

  std::map<std::string, views::View*> views_by_item_id_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_CONTAINER_H_

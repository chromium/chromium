// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_SCREEN_CAPTURES_SECTION_H_
#define ASH_SYSTEM_HOLDING_SPACE_SCREEN_CAPTURES_SECTION_H_

#include <map>

#include "ash/system/holding_space/holding_space_item_views_container.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class HoldingSpaceItemView;

// Section for screen captures in the `RecentFilesContainer`.
class ScreenCapturesSection : public HoldingSpaceItemViewsContainer {
 public:
  explicit ScreenCapturesSection(HoldingSpaceItemViewDelegate* delegate);
  ScreenCapturesSection(const ScreenCapturesSection& other) = delete;
  ScreenCapturesSection& operator=(const ScreenCapturesSection& other) = delete;
  ~ScreenCapturesSection() override;

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
  void AddHoldingSpaceScreenCaptureView(const HoldingSpaceItem* item);
  void AddHoldingSpaceDownloadView(const HoldingSpaceItem* item);
  void OnScreenCapturesContainerViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details);
  void OnDownloadsContainerViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details);

  // Owned by view hierarchy.
  views::View* container_ = nullptr;
  views::Label* label_ = nullptr;

  std::map<std::string, HoldingSpaceItemView*> views_by_item_id_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_SCREEN_CAPTURES_SECTION_H_

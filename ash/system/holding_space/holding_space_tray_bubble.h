// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_BUBBLE_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_BUBBLE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/holding_space/holding_space_item_view_delegate.h"
#include "ash/system/tray/tray_bubble_wrapper.h"

namespace ash {

class HoldingSpaceTray;
class PinnedFilesContainer;
class RecentFilesContainer;

class ASH_EXPORT HoldingSpaceTrayBubble {
 public:
  HoldingSpaceTrayBubble(HoldingSpaceTray* holding_space_tray,
                         bool show_by_click);
  HoldingSpaceTrayBubble(const HoldingSpaceTrayBubble&) = delete;
  HoldingSpaceTrayBubble& operator=(const HoldingSpaceTrayBubble&) = delete;
  ~HoldingSpaceTrayBubble();

  void AnchorUpdated();

  TrayBubbleView* GetBubbleView();
  views::Widget* GetBubbleWidget();

 private:
  // Return the maximum height available for the holding space bubble.
  int CalculateMaxHeight() const;

  // The owner of this class.
  HoldingSpaceTray* const holding_space_tray_;

  // The singleton delegate for `HoldingSpaceItemView`s that implements support
  // for context menu, drag-and-drop, and multiple selection.
  HoldingSpaceItemViewDelegate delegate_;

  PinnedFilesContainer* pinned_files_container_ = nullptr;
  RecentFilesContainer* recent_files_container_ = nullptr;

  std::unique_ptr<TrayBubbleWrapper> bubble_wrapper_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_BUBBLE_H_

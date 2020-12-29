// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_BUBBLE_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_BUBBLE_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/system/holding_space/holding_space_item_view_delegate.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"

namespace ash {

class HoldingSpaceTray;
class HoldingSpaceTrayChildBubble;

// The bubble associated with the `HoldingSpaceTray`.
class ASH_EXPORT HoldingSpaceTrayBubble : public ScreenLayoutObserver,
                                          public ShelfObserver,
                                          public TabletModeObserver {
 public:
  HoldingSpaceTrayBubble(HoldingSpaceTray* holding_space_tray,
                         bool show_by_click);
  HoldingSpaceTrayBubble(const HoldingSpaceTrayBubble&) = delete;
  HoldingSpaceTrayBubble& operator=(const HoldingSpaceTrayBubble&) = delete;
  ~HoldingSpaceTrayBubble() override;

  void AnchorUpdated();

  TrayBubbleView* GetBubbleView();
  views::Widget* GetBubbleWidget();

 private:
  class ChildBubbleContainer;

  // Return the maximum height available for the holding space bubble.
  int CalculateMaxHeight() const;

  void UpdateBubbleBounds();

  // ScreenLayoutObserver:
  void OnDisplayConfigurationChanged() override;

  // ShelfObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // The owner of this class.
  HoldingSpaceTray* const holding_space_tray_;

  // The singleton delegate for `HoldingSpaceItemView`s that implements support
  // for context menu, drag-and-drop, and multiple selection.
  HoldingSpaceItemViewDelegate delegate_;

  // Views owned by view hierarchy.
  ChildBubbleContainer* child_bubble_container_;
  std::vector<HoldingSpaceTrayChildBubble*> child_bubbles_;
  std::unique_ptr<ui::EventHandler> event_filter_;

  std::unique_ptr<TrayBubbleWrapper> bubble_wrapper_;

  ScopedObserver<Shelf, ShelfObserver> shelf_observer_{this};
  ScopedObserver<TabletModeController, TabletModeObserver>
      tablet_mode_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_BUBBLE_H_

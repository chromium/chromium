// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_BUBBLE_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_BUBBLE_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/system/holding_space/holding_space_view_delegate.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/display/display_observer.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace ash {

class HoldingSpaceTray;
class HoldingSpaceTrayChildBubble;

// The bubble associated with the `HoldingSpaceTray`.
class ASH_EXPORT HoldingSpaceTrayBubble : public ScreenLayoutObserver,
                                          public ShelfObserver,
                                          public display::DisplayObserver {
 public:
  explicit HoldingSpaceTrayBubble(HoldingSpaceTray* holding_space_tray);
  HoldingSpaceTrayBubble(const HoldingSpaceTrayBubble&) = delete;
  HoldingSpaceTrayBubble& operator=(const HoldingSpaceTrayBubble&) = delete;
  ~HoldingSpaceTrayBubble() override;

  void Init();

  void AnchorUpdated();

  TrayBubbleView* GetBubbleView();
  views::Widget* GetBubbleWidget();

  // Returns all holding space item views in the bubble. Views are returned in
  // top-to-bottom, left-to-right order (or mirrored for RTL).
  std::vector<HoldingSpaceItemView*> GetHoldingSpaceItemViews();

  // Returns the `holding_space_tray_` associated with this bubble.
  HoldingSpaceTray* tray() { return holding_space_tray_; }

 private:
  class ChildBubbleContainer;

  // Return the maximum height available for the top-level holding space bubble
  // and child bubble container respectively.
  int CalculateTopLevelBubbleMaxHeight() const;
  int CalculateChildBubbleContainerMaxHeight() const;

  void UpdateBubbleBounds();

  // ScreenLayoutObserver:
  void OnDidApplyDisplayChanges() override;

  // ShelfObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // The owner of this class.
  const raw_ptr<HoldingSpaceTray, DanglingUntriaged> holding_space_tray_;

  // The singleton delegate for holding space views that implements support
  // for context menu, drag-and-drop, and multiple selection.
  HoldingSpaceViewDelegate delegate_{this};

  // Views owned by view hierarchy.
  raw_ptr<views::View> header_ = nullptr;
  raw_ptr<ChildBubbleContainer> child_bubble_container_ = nullptr;
  std::vector<raw_ptr<HoldingSpaceTrayChildBubble, VectorExperimental>>
      child_bubbles_;

  std::unique_ptr<TrayBubbleWrapper> bubble_wrapper_;
  std::unique_ptr<ui::EventHandler> event_handler_;

  base::ScopedObservation<Shelf, ShelfObserver> shelf_observation_{this};
  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_BUBBLE_H_

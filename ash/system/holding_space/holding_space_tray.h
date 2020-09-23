// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/memory/weak_ptr.h"

namespace gfx {
class Point;
}

namespace views {
class ImageView;
}

namespace ash {

class PinnedFilesContainer;
class RecentFilesContainer;
class TrayBubbleWrapper;

// The HoldingSpaceTray shows the tray button in the bottom area of the screen.
// This class also controls the lifetime for all of the tools available in the
// palette. HoldingSpaceTray has one instance per-display.
class ASH_EXPORT HoldingSpaceTray : public TrayBackgroundView {
 public:
  explicit HoldingSpaceTray(Shelf* shelf);
  HoldingSpaceTray(const HoldingSpaceTray& other) = delete;
  HoldingSpaceTray& operator=(const HoldingSpaceTray& other) = delete;
  ~HoldingSpaceTray() override;

  // Returns true if the tray contains the given point. This is useful
  // for determining if an event should be propagated through to the palette.
  bool ContainsPointInScreen(const gfx::Point& point);

  // TrayBackgroundView:
  void ClickedOutsideBubble() override;
  base::string16 GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void AnchorUpdated() override;
  void UpdateAfterLoginStatusChange() override;
  bool PerformAction(const ui::Event& event) override;
  void CloseBubble() override;
  void ShowBubble(bool show_by_click) override;
  TrayBubbleView* GetBubbleView() override;
  const char* GetClassName() const override;

 private:
  // TrayBubbleView::Delegate:
  base::string16 GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  std::unique_ptr<TrayBubbleWrapper> bubble_;

  PinnedFilesContainer* pinned_files_container_ = nullptr;
  RecentFilesContainer* recent_files_container_ = nullptr;

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_ = nullptr;

  base::WeakPtrFactory<HoldingSpaceTray> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_H_

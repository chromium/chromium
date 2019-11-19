// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_TOOLTIP_PREVIEW_BUBBLE_H_
#define ASH_SHELF_SHELF_TOOLTIP_PREVIEW_BUBBLE_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_bubble.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/window_preview.h"
#include "ash/wm/window_mirror_view.h"
#include "base/timer/timer.h"
#include "ui/aura/window.h"
#include "ui/views/controls/label.h"

namespace ash {

// The implementation of tooltip bubbles for the shelf item.
class ASH_EXPORT ShelfTooltipPreviewBubble : public ShelfBubble,
                                             public WindowPreview::Delegate {
 public:
  ShelfTooltipPreviewBubble(views::View* anchor,
                            const std::vector<aura::Window*>& windows,
                            ShelfTooltipManager* manager,
                            ShelfAlignment alignment,
                            SkColor background_color);
  ~ShelfTooltipPreviewBubble() override;

 private:
  // Removes the given preview from the list of previewed windows.
  void RemovePreview(WindowPreview* preview);

  // BubbleDialogDelegateView overrides:
  gfx::Rect GetBubbleBounds() override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // ShelfBubble:
  bool ShouldCloseOnPressDown() override;
  bool ShouldCloseOnMouseExit() override;

  // WindowPreview::Delegate:
  float GetMaxPreviewRatio() const override;
  void OnPreviewDismissed(WindowPreview* preview) override;
  void OnPreviewActivated(WindowPreview* preview) override;

  void DismissAfterDelay();
  void Dismiss();

  std::vector<WindowPreview*> previews_;

  ShelfTooltipManager* manager_;
  base::OneShotTimer dismiss_timer_;

  const ShelfAlignment shelf_alignment_;

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipPreviewBubble);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TOOLTIP_PREVIEW_BUBBLE_H_

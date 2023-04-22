// Copyright 2018 The Chromium Authors
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
#include "base/memory/raw_ptr.h"
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
                            ShelfAlignment alignment);

  ShelfTooltipPreviewBubble(const ShelfTooltipPreviewBubble&) = delete;
  ShelfTooltipPreviewBubble& operator=(const ShelfTooltipPreviewBubble&) =
      delete;

  ~ShelfTooltipPreviewBubble() override;

 private:
  // Removes the given preview from the list of previewed windows.
  void RemovePreview(WindowPreview* preview);

  // BubbleDialogDelegateView overrides:
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

  raw_ptr<ShelfTooltipManager, ExperimentalAsh> manager_;
  base::OneShotTimer dismiss_timer_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TOOLTIP_PREVIEW_BUBBLE_H_

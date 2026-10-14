// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_HOVER_TEXT_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_HOVER_TEXT_BUBBLE_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace display {
class Display;
}  // namespace display

namespace ash {

class HoverTextBubbleView;

// Properties for updating the native Ash Hover Text bubble widget.
struct HoverTextBubbleProps {
  std::u16string text;
  gfx::Rect anchor_rect;
  double font_scale = 2.0;
  HoverTextFontStyle font_style = HoverTextFontStyle::kSystemSans;
  bool visible = false;
};

// Manages the lifecycle and screen positioning of the Hover Text bubble
// widget.
class ASH_EXPORT HoverTextBubbleController : public views::WidgetObserver {
 public:
  HoverTextBubbleController();
  HoverTextBubbleController(const HoverTextBubbleController&) = delete;
  HoverTextBubbleController& operator=(const HoverTextBubbleController&) =
      delete;
  ~HoverTextBubbleController() override;

  // Updates the bubble's visibility, text, anchor bounds, and font styling.
  void UpdateBubble(const HoverTextBubbleProps& props);

  // Hides the bubble if it is currently visible.
  void Hide();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  bool IsBubbleVisibleForTesting() const;
  std::u16string GetBubbleTextForTesting() const;
  gfx::Rect GetBubbleBoundsInScreenForTesting() const;
  HoverTextBubbleView* bubble_view_for_testing() const { return bubble_view_; }
  views::Widget* widget_for_testing() const { return widget_.get(); }

 private:
  void EnsureInitialize(aura::Window* root_window);
  void OnWidgetClosed(views::Widget::ClosedReason closed_reason);
  void ResetBubble();
  gfx::Rect ComputeBubbleBounds(const display::Display& display,
                                const gfx::Rect& anchor_rect,
                                const gfx::Size& preferred_size) const;

  raw_ptr<HoverTextBubbleView> bubble_view_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_HOVER_TEXT_BUBBLE_CONTROLLER_H_

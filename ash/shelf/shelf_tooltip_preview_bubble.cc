// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_preview_bubble.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/window_preview_view.h"
#include "base/bind.h"
#include "base/containers/cxx20_erase.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/display/screen.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

// The delay after which a preview bubble gets dismissed (after the mouse
// has gone away for instance).s
constexpr int kPreviewBubbleDismissDelay = 350;

// The padding inside the tooltip.
constexpr int kTooltipPaddingTop = 8;
constexpr int kTooltipPaddingBottom = 16;
constexpr int kTooltipPaddingLeftRight = 16;

// The padding between individual previews.
constexpr int kPreviewPadding = 10;

// The border radius of the whole bubble
constexpr int kPreviewBubbleBorderRadius = 16;

ShelfTooltipPreviewBubble::ShelfTooltipPreviewBubble(
    views::View* anchor,
    const std::vector<aura::Window*>& windows,
    ShelfTooltipManager* manager,
    ShelfAlignment alignment)
    : ShelfBubble(anchor, alignment), manager_(manager) {
  set_border_radius(kPreviewBubbleBorderRadius);
  SetCanActivate(false);
  set_close_on_deactivate(false);
  // The parent class sets non-zero margins. Reset them to zero.
  set_margins(gfx::Insets());
  // We hide this tooltip on mouse exit, so we want to get enter/exit events
  // at this level, even for subviews.
  SetNotifyEnterExitOnChild(true);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(kTooltipPaddingTop, kTooltipPaddingLeftRight,
                        kTooltipPaddingBottom, kTooltipPaddingLeftRight),
      kPreviewPadding));

  for (auto* window : windows) {
    WindowPreview* preview = new WindowPreview(window, this);
    AddChildView(preview);
    previews_.push_back(preview);
  }

  CreateBubble();
  CollisionDetectionUtils::IgnoreWindowForCollisionDetection(
      GetWidget()->GetNativeWindow());
}

ShelfTooltipPreviewBubble::~ShelfTooltipPreviewBubble() = default;

void ShelfTooltipPreviewBubble::RemovePreview(WindowPreview* to_remove) {
  base::Erase(previews_, to_remove);
  RemoveChildView(to_remove);
  // If we don't have any previews left, close the tooltip. Bypass
  // considerations of where the mouse pointer is when this happens: we don't
  // want to show an empty tooltip even if the mouse is on it.
  if (previews_.empty())
    manager_->Close();
}

void ShelfTooltipPreviewBubble::OnMouseExited(const ui::MouseEvent& event) {
  DismissAfterDelay();
}

bool ShelfTooltipPreviewBubble::ShouldCloseOnPressDown() {
  return false;
}

bool ShelfTooltipPreviewBubble::ShouldCloseOnMouseExit() {
  return false;
}

float ShelfTooltipPreviewBubble::GetMaxPreviewRatio() const {
  float max_ratio = ShelfConfig::Get()->shelf_tooltip_preview_min_ratio();
  for (WindowPreview* window : previews_) {
    gfx::Size mirror_size = window->preview_view()->CalculatePreferredSize();
    float ratio = static_cast<float>(mirror_size.width()) /
                  static_cast<float>(mirror_size.height());
    max_ratio = std::max(max_ratio, ratio);
  }
  return std::min(max_ratio,
                  ShelfConfig::Get()->shelf_tooltip_preview_max_ratio());
}

void ShelfTooltipPreviewBubble::DismissAfterDelay() {
  dismiss_timer_.Start(FROM_HERE,
                       base::Milliseconds(kPreviewBubbleDismissDelay),
                       base::BindOnce(&ShelfTooltipPreviewBubble::Dismiss,
                                      base::Unretained(this)));
}

void ShelfTooltipPreviewBubble::Dismiss() {
  dismiss_timer_.Stop();

  const auto cursor_position =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  // Cancel dismissal if the mouse is within our bounds again, or if it's
  // within the anchor's bounds. That way the preview tooltip will remain
  // shown if the mouse goes between the bubble and its anchor.
  if (GetBoundsInScreen().Contains(cursor_position) ||
      GetAnchorRect().Contains(cursor_position)) {
    return;
  }
  manager_->Close();
}

void ShelfTooltipPreviewBubble::OnPreviewDismissed(WindowPreview* preview) {
  RemovePreview(preview);
}

void ShelfTooltipPreviewBubble::OnPreviewActivated(WindowPreview* preview) {
  // Always close the tooltip when a window has been focused. Bypass
  // considerations of where the mouse pointer is when this happens.
  manager_->Close();
}

}  // namespace ash

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/hover_text_bubble_controller.h"

#include <algorithm>
#include <string>

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/accessibility/hover_text_bubble_view.h"
#include "base/functional/bind.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace ash {

namespace {

// Vertical spacing in DIP between the target node / pointer anchor and the
// Hover Text bubble so the bubble does not occlude the hovered element.
constexpr int kAnchorVerticalOffsetDip = 16;

}  // namespace

HoverTextBubbleController::HoverTextBubbleController() = default;

HoverTextBubbleController::~HoverTextBubbleController() {
  ResetBubble();
}

void HoverTextBubbleController::UpdateBubble(
    const HoverTextBubbleProps& props) {
  if (!props.visible || props.text.empty()) {
    Hide();
    return;
  }

  const display::Display display =
      display::Screen::Get()->GetDisplayMatching(props.anchor_rect);
  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display.id());
  if (!root_window) {
    root_window = Shell::GetPrimaryRootWindow();
  }

  EnsureInitialize(root_window);
  if (!bubble_view_ || !widget_) {
    return;
  }
  bubble_view_->Update(props.text, props.font_scale, props.font_style);

  const gfx::Size preferred_size =
      widget_->non_client_view()
          ? widget_->non_client_view()->GetPreferredSize()
          : bubble_view_->GetPreferredSize();
  const gfx::Rect bubble_bounds =
      ComputeBubbleBounds(display, props.anchor_rect, preferred_size);
  widget_->SetBounds(bubble_bounds);

  if (!widget_->IsVisible()) {
    widget_->ShowInactive();
  }
}

void HoverTextBubbleController::Hide() {
  if (widget_) {
    widget_->Hide();
  }
}

void HoverTextBubbleController::OnWidgetDestroying(views::Widget* widget) {
  if (widget != widget_.get()) {
    return;
  }
  widget_observation_.Reset();
  bubble_view_ = nullptr;
}

bool HoverTextBubbleController::IsBubbleVisibleForTesting() const {
  return widget_ && widget_->IsVisible();
}

std::u16string HoverTextBubbleController::GetBubbleTextForTesting() const {
  return bubble_view_ ? bubble_view_->GetTextForTesting()  // IN-TEST
                      : std::u16string();
}

gfx::Rect HoverTextBubbleController::GetBubbleBoundsInScreenForTesting() const {
  return widget_ ? widget_->GetWindowBoundsInScreen() : gfx::Rect();
}

void HoverTextBubbleController::EnsureInitialize(aura::Window* root_window) {
  if (!widget_ || widget_->IsClosed()) {
    ResetBubble();
    bubble_view_ = new HoverTextBubbleView();
    widget_ = views::BubbleDialogDelegate::CreateBubble(
        bubble_view_, base::BindOnce(&HoverTextBubbleController::OnWidgetClosed,
                                     base::Unretained(this)));
    widget_observation_.Observe(widget_.get());
    widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingUIElement);

    if (views::View* root_view = widget_->GetRootView()) {
      root_view->SetFocusBehavior(views::View::FocusBehavior::NEVER);
      root_view->SetCanProcessEventsWithinSubtree(false);
      root_view->GetViewAccessibility().SetIsIgnored(true);
      root_view->GetViewAccessibility().SetIsLeaf(true);
    }

    if (aura::Window* native_window = widget_->GetNativeView()) {
      // Ignore pointer hit-testing so clicks and hovers pass transparently
      // through the bubble to underlying windows, and suppress the Aura window
      // from `AutomationManagerAura` so bounds updates do not emit
      // `ax::mojom::Event::kLocationChanged` mutations.
      native_window->SetEventTargetingPolicy(aura::EventTargetingPolicy::kNone);
      native_window->SetProperty(ui::kAXConsiderInvisibleAndIgnoreChildren,
                                 true);
      native_window->SetProperty(ui::kAXRoleOverride, ax::mojom::Role::kNone);
    }
  }

  // Reparent the bubble widget to the target display's accessibility bubble
  // container if the user moved the pointer across displays.
  // `aura::Window::AddChild()` automatically removes the window (and its
  // `ui::Layer`) from its previous parent container before attaching it to
  // `target_container`.
  if (root_window && widget_->GetNativeWindow()) {
    aura::Window* target_container = Shell::GetContainer(
        root_window, kShellWindowId_AccessibilityBubbleContainer);
    if (target_container &&
        widget_->GetNativeWindow()->parent() != target_container) {
      target_container->AddChild(widget_->GetNativeWindow());
    }
  }
}

void HoverTextBubbleController::OnWidgetClosed(
    views::Widget::ClosedReason closed_reason) {
  ResetBubble();
}

void HoverTextBubbleController::ResetBubble() {
  widget_observation_.Reset();
  bubble_view_ = nullptr;
  widget_.reset();
}

gfx::Rect HoverTextBubbleController::ComputeBubbleBounds(
    const display::Display& display,
    const gfx::Rect& anchor_rect,
    const gfx::Size& preferred_size) const {
  const gfx::Rect work_area = display.work_area();

  // Place the bubble `kAnchorVerticalOffsetDip` below `anchor_rect` by default,
  // centered horizontally on `anchor_rect.CenterPoint().x()`.
  const int candidate_x =
      anchor_rect.CenterPoint().x() - preferred_size.width() / 2;
  const int candidate_y = anchor_rect.bottom() + kAnchorVerticalOffsetDip;

  gfx::Rect candidate_rect(gfx::Point(candidate_x, candidate_y),
                           preferred_size);
  candidate_rect.AdjustToFit(work_area);

  // If clamping to `work_area` causes `candidate_rect` to intersect
  // `anchor_rect` (e.g., when hovering near the bottom shelf), flip the bubble
  // above `anchor_rect` and re-clamp to `work_area`.
  if (candidate_rect.Intersects(anchor_rect)) {
    candidate_rect.set_y(anchor_rect.y() - preferred_size.height() -
                         kAnchorVerticalOffsetDip);
    candidate_rect.AdjustToFit(work_area);
  }

  return candidate_rect;
}

}  // namespace ash

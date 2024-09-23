// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_scroll_bubble_controller.h"

#include "ash/bubble/bubble_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event_utils.h"
#include "ui/views/border.h"

namespace ash {

namespace {
// Autoclick scroll menu constants.
constexpr int kAutoclickScrollMenuSizeDips = 192;
const int kScrollPointBufferDips = 16;
const int kScrollRectBufferDips = 8;

struct Position {
  int distance;
  views::BubbleBorder::Arrow arrow;
  bool is_horizontal;
};

bool comparePositions(Position first, Position second) {
  return first.distance < second.distance;
}
}  // namespace

AutoclickScrollBubbleController::AutoclickScrollBubbleController() = default;

AutoclickScrollBubbleController::~AutoclickScrollBubbleController() {
  if (bubble_widget_ && !bubble_widget_->IsClosed())
    bubble_widget_->CloseNow();

  bubble_view_ = nullptr;
  scroll_view_ = nullptr;
  bubble_widget_ = nullptr;
}

void AutoclickScrollBubbleController::UpdateAnchorRect(
    gfx::Rect rect,
    views::BubbleBorder::Arrow alignment) {
  menu_bubble_rect_ = rect;
  menu_bubble_alignment_ = alignment;
  if (set_scroll_rect_ || !bubble_view_)
    return;
  bubble_view_->UpdateAnchorRect(rect, alignment);
}

void AutoclickScrollBubbleController::SetScrollPosition(
    gfx::Rect scroll_bounds_in_dips,
    const gfx::Point& scroll_point_in_dips) {
  // TODO(katie): Support multiple displays.

  if (!bubble_view_)
    return;

  // Adjust the insets to be the same on all sides, so that when the bubble
  // lays out it isn't too close on the top or bottom.
  bubble_view_->UpdateInsets(
      gfx::Insets::VH(kBubbleMenuPadding, kBubbleMenuPadding));

  aura::Window* window = Shell::GetPrimaryRootWindow();
  gfx::Rect work_area =
      WorkAreaInsets::ForWindow(window)->user_work_area_bounds();

  // If the on-screen width and height of the scroll bounds are larger than a
  // threshold size, simply offset the scroll menu bubble from the scroll point
  // position. This ensures the scroll bubble does not end up too far away from
  // the user's scroll point.
  gfx::Rect on_screen_scroll_bounds(scroll_bounds_in_dips);
  on_screen_scroll_bounds.Intersect(work_area);
  if (on_screen_scroll_bounds.width() > 2 * kAutoclickScrollMenuSizeDips &&
      on_screen_scroll_bounds.height() > 2 * kAutoclickScrollMenuSizeDips) {
    set_scroll_rect_ = true;
    gfx::Rect anchor =
        gfx::Rect(scroll_point_in_dips.x(), scroll_point_in_dips.y(), 0, 0);
    // Buffer around the point so that the scroll bubble does not overlap it.
    // ScrollBubbleController will automatically layout to avoid edges.
    // Aims to lay out like the context menu, at 16x16 to the bottom right.
    anchor.Inset(gfx::Insets::VH(0, -kScrollPointBufferDips));
    bubble_view_->UpdateAnchorRect(anchor,
                                   views::BubbleBorder::Arrow::LEFT_TOP);
    return;
  }

  // Otherwise, the scrollable area bounds are small compared to the scroll
  // scroll bubble, so we should not overlap the scroll bubble if possible.
  // Try to show the scroll bubble on the side of the rect closest to the point.
  // Determine whether there will be space to show the scroll bubble between the
  // scroll bounds and display bounds.
  work_area.Inset(kAutoclickScrollMenuSizeDips);
  scroll_bounds_in_dips.Inset(-kScrollRectBufferDips);
  bool fits_left = scroll_bounds_in_dips.x() > work_area.x();
  bool fits_right = scroll_bounds_in_dips.right() < work_area.right();
  bool fits_above = scroll_bounds_in_dips.y() > work_area.y();
  bool fits_below = scroll_bounds_in_dips.bottom() < work_area.bottom();

  // The scroll bubble will fit outside the bounds of the scrollable area.
  // Determine its exact position based on the scroll point:
  // First, try to lay out the scroll bubble on the side of the scroll bounds
  // closest to the scroll point.
  //
  //   ------------------
  //   |                |
  //   |               *|  <-- Here, the scroll point is closest to the right
  //   |                |      edge so the bubble should be laid out on the
  //   |                |      right of the scroll bounds.
  //   ------------------
  //
  std::vector<Position> positions;
  if (fits_right) {
    positions.push_back(
        {scroll_bounds_in_dips.right() - scroll_point_in_dips.x(),
         // Put it on the right. Note that in RTL languages right and left
         // arrows
         // are swapped.
         base::i18n::IsRTL() ? views::BubbleBorder::Arrow::RIGHT_CENTER
                             : views::BubbleBorder::Arrow::LEFT_CENTER,
         true});
  }
  if (fits_left) {
    positions.push_back({scroll_point_in_dips.x() - scroll_bounds_in_dips.x(),
                         // Put it on the left. Note that in RTL languages right
                         // and left arrows are swapped.
                         base::i18n::IsRTL()
                             ? views::BubbleBorder::Arrow::LEFT_CENTER
                             : views::BubbleBorder::Arrow::RIGHT_CENTER,
                         true});
  }
  if (fits_below) {
    positions.push_back(
        {scroll_bounds_in_dips.bottom() - scroll_point_in_dips.y(),
         views::BubbleBorder::Arrow::TOP_CENTER, false});
  }
  if (fits_above) {
    positions.push_back({scroll_point_in_dips.y() - scroll_bounds_in_dips.y(),
                         views::BubbleBorder::Arrow::BOTTOM_CENTER, false});
  }

  // If the scroll bubble doesn't fit between the scroll bounds and display
  // edges, re-pin the scroll bubble to the menu bubble.
  // This may not ever happen except on low-resolution screens.
  set_scroll_rect_ = !positions.empty();
  if (!set_scroll_rect_) {
    bubble_view_->UpdateInsets(gfx::Insets::TLBR(
        0, kBubbleMenuPadding, kBubbleMenuPadding, kBubbleMenuPadding));
    UpdateAnchorRect(menu_bubble_rect_, menu_bubble_alignment_);
    return;
  }

  // Sort positions based on distance.
  std::stable_sort(positions.begin(), positions.end(), comparePositions);

  // Position on the optimal side depending on the shortest distance.
  if (positions.front().is_horizontal) {
    // Center it vertically on the scroll point.
    bubble_view_->UpdateAnchorRect(
        gfx::Rect(scroll_bounds_in_dips.x(), scroll_point_in_dips.y(),
                  scroll_bounds_in_dips.width(), 0),
        positions.at(0).arrow);
  } else {
    // Center horizontally on scroll point.
    bubble_view_->UpdateAnchorRect(
        gfx::Rect(scroll_point_in_dips.x(), scroll_bounds_in_dips.y(), 0,
                  scroll_bounds_in_dips.height()),
        positions.at(0).arrow);
  }
}

void AutoclickScrollBubbleController::ShowBubble(
    gfx::Rect anchor_rect,
    views::BubbleBorder::Arrow alignment) {
  // Ignore if bubble widget already exists.
  if (bubble_widget_)
    return;

  DCHECK(!bubble_view_);

  TrayBubbleView::InitParams init_params;
  init_params.delegate = GetWeakPtr();
  // Anchor within the overlay container.
  init_params.parent_window =
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_AccessibilityBubbleContainer);
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = anchor_rect;
  // The widget's shadow is drawn below and on the sides of the scroll view.
  // Do not inset the top, so that when the scroll bubble is shown below the
  // menu bubble it lays out directly below the menu bubble's shadow, at a
  // height of kBubbleMenuPadding.
  init_params.insets = gfx::Insets::TLBR(
      0, kBubbleMenuPadding, kBubbleMenuPadding, kBubbleMenuPadding);
  init_params.preferred_width = kAutoclickScrollMenuSizeDips;
  init_params.max_height = kAutoclickScrollMenuSizeDips;
  init_params.translucent = true;
  init_params.type = TrayBubbleView::TrayBubbleType::kAccessibilityBubble;

  bubble_view_ = new AutoclickScrollBubbleView(init_params);
  bubble_view_->SetArrow(alignment);

  scroll_view_ = new AutoclickScrollView();
  scroll_view_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kUnifiedTopShortcutSpacing, 0, 0, 0)));
  bubble_view_->AddChildView(scroll_view_.get());

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
      bubble_widget_->GetNativeWindow(),
      CollisionDetectionUtils::RelativePriority::kAutomaticClicksScrollMenu);
  bubble_view_->InitializeAndShowBubble();
}

void AutoclickScrollBubbleController::CloseBubble() {
  if (!bubble_widget_ || bubble_widget_->IsClosed())
    return;
  bubble_widget_->Close();
}

void AutoclickScrollBubbleController::SetBubbleVisibility(bool is_visible) {
  if (!bubble_widget_)
    return;

  if (is_visible)
    bubble_widget_->Show();
  else
    bubble_widget_->Hide();
}

void AutoclickScrollBubbleController::ClickOnBubble(gfx::Point location_in_dips,
                                                    int mouse_event_flags) {
  if (!bubble_widget_ || !bubble_view_)
    return;

  // Change the event location bounds to be relative to the menu bubble.
  location_in_dips -= bubble_view_->GetBoundsInScreen().OffsetFromOrigin();

  // Generate synthesized mouse events for the click.
  const ui::MouseEvent press_event(
      ui::EventType::kMousePressed, location_in_dips, location_in_dips,
      ui::EventTimeForNow(), mouse_event_flags | ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);
  const ui::MouseEvent release_event(
      ui::EventType::kMouseReleased, location_in_dips, location_in_dips,
      ui::EventTimeForNow(), mouse_event_flags | ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);

  // Send the press/release events to the widget's root view for processing.
  bubble_widget_->GetRootView()->OnMousePressed(press_event);
  bubble_widget_->GetRootView()->OnMouseReleased(release_event);
}

bool AutoclickScrollBubbleController::ContainsPointInScreen(
    const gfx::Point& point) {
  return bubble_view_ && bubble_view_->GetBoundsInScreen().Contains(point);
}

void AutoclickScrollBubbleController::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;
  scroll_view_ = nullptr;
}

std::u16string AutoclickScrollBubbleController::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_AUTOCLICK_SCROLL_BUBBLE);
}

void AutoclickScrollBubbleController::HideBubble(
    const TrayBubbleView* bubble_view) {
  // This function is currently not unused for bubbles of type
  // `kAccessibilityBubble`, so can leave this empty.
}

}  // namespace ash

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/anchored_nudge.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/toast/nudge_constants.h"
#include "ash/system/toast/system_nudge_view.h"
#include "ash/wm/work_area_insets.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace ash {

namespace {

// Offsets the bottom of work area to account for the current hotseat state.
void AdjustWorkAreaBoundsForHotseatState(const HotseatWidget* hotseat_widget,
                                         gfx::Rect& work_area_bounds) {
  switch (hotseat_widget->state()) {
    case HotseatState::kExtended:
      work_area_bounds.set_height(work_area_bounds.height() -
                                  hotseat_widget->GetHotseatSize() -
                                  ShelfConfig::Get()->hotseat_bottom_padding());
      break;
    case HotseatState::kShownHomeLauncher:
      work_area_bounds.set_height(hotseat_widget->GetTargetBounds().y() -
                                  work_area_bounds.y());
      break;
    case HotseatState::kHidden:
    case HotseatState::kShownClamshell:
    case HotseatState::kNone:
      // Do nothing.
      return;
  }
}

// Returns true if the provided arrow is located at a corner.
bool CalculateIsCornerAnchored(views::BubbleBorder::Arrow arrow) {
  switch (arrow) {
    case views::BubbleBorder::Arrow::TOP_LEFT:
    case views::BubbleBorder::Arrow::TOP_RIGHT:
    case views::BubbleBorder::Arrow::BOTTOM_LEFT:
    case views::BubbleBorder::Arrow::BOTTOM_RIGHT:
    case views::BubbleBorder::Arrow::LEFT_TOP:
    case views::BubbleBorder::Arrow::RIGHT_TOP:
    case views::BubbleBorder::Arrow::LEFT_BOTTOM:
    case views::BubbleBorder::Arrow::RIGHT_BOTTOM:
      return true;
    default:
      return false;
  }
}

gfx::Point GetAnchorPoint(views::Widget* anchor_widget,
                          views::BubbleBorder::Arrow corner) {
  const bool is_rtl = base::i18n::IsRTL();
  auto bounds = anchor_widget->GetWindowBoundsInScreen();

  const gfx::Point bottom_left =
      gfx::Point(bounds.x() + kBubbleBorderInsets.left(),
                 bounds.bottom() - kBubbleBorderInsets.bottom());
  const gfx::Point bottom_right =
      gfx::Point(bounds.right() - kBubbleBorderInsets.right(),
                 bounds.bottom() - kBubbleBorderInsets.bottom());

  // Only support corners at the bottom of the widget.
  switch (corner) {
    case views::BubbleBorder::Arrow::BOTTOM_LEFT:
    case views::BubbleBorder::Arrow::LEFT_BOTTOM:
      return is_rtl ? bottom_right : bottom_left;
    case views::BubbleBorder::Arrow::BOTTOM_RIGHT:
    case views::BubbleBorder::Arrow::RIGHT_BOTTOM:
      return is_rtl ? bottom_left : bottom_right;
    default:
      return is_rtl ? bottom_right : bottom_left;
  }
}

}  // namespace

AnchoredNudge::AnchoredNudge(
    AnchoredNudgeData& nudge_data,
    base::RepeatingCallback<void(/*has_hover_or_focus=*/bool)>
        hover_or_focus_changed_callback)
    : views::BubbleDialogDelegateView(nudge_data.GetAnchorView(),
                                      nudge_data.arrow,
                                      views::BubbleBorder::NO_SHADOW),
      id_(nudge_data.id),
      catalog_name_(nudge_data.catalog_name),
      anchored_to_shelf_(nudge_data.anchored_to_shelf),
      is_corner_anchored_(CalculateIsCornerAnchored(nudge_data.arrow)),
      set_anchor_view_as_parent_(nudge_data.set_anchor_view_as_parent),
      anchor_widget_(nudge_data.anchor_widget),
      anchor_widget_corner_(nudge_data.arrow),
      click_callback_(std::move(nudge_data.click_callback)),
      dismiss_callback_(std::move(nudge_data.dismiss_callback)) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_color(SK_ColorTRANSPARENT);
  set_margins(gfx::Insets());
  set_close_on_deactivate(false);
  set_highlight_button_when_shown(nudge_data.highlight_anchor_button);
  SetLayoutManager(std::make_unique<views::FlexLayout>());
  system_nudge_view_ = AddChildView(std::make_unique<SystemNudgeView>(
      nudge_data, std::move(hover_or_focus_changed_callback)));

  // Make nudge not focus traversable if it does not have any buttons.
  if (nudge_data.primary_button_text.empty()) {
    set_focus_traversable_from_anchor_view(false);
  }

  if (anchored_to_shelf_ || !GetAnchorView()) {
    Shell::Get()->AddShellObserver(this);
  }

  if (!nudge_data.announce_chromevox) {
    SetAccessibleWindowRole(ax::mojom::Role::kNone);
  }
}

AnchoredNudge::~AnchoredNudge() {
  if (!dismiss_callback_.is_null()) {
    std::move(dismiss_callback_).Run();
  }

  if (anchored_to_shelf_) {
    disable_shelf_auto_hide_.reset();
  }

  if (anchored_to_shelf_ || !GetAnchorView()) {
    Shell::Get()->RemoveShellObserver(this);
  }

  anchor_widget_ = nullptr;
}

gfx::Rect AnchoredNudge::GetBubbleBounds() {
  auto* root_window = GetWidget()->GetNativeWindow();

  // This can happen during destruction.
  if (!root_window) {
    return gfx::Rect();
  }

  gfx::Rect bubble_bounds = views::BubbleDialogDelegateView::GetBubbleBounds();
  if (anchor_widget_) {
    return bubble_bounds;
  }

  gfx::Rect work_area_bounds =
      WorkAreaInsets::ForWindow(root_window)->user_work_area_bounds();

  auto* hotseat_widget =
      RootWindowController::ForWindow(root_window)->shelf()->hotseat_widget();
  if (hotseat_widget) {
    AdjustWorkAreaBoundsForHotseatState(hotseat_widget, work_area_bounds);
  }
  bubble_bounds.AdjustToFit(work_area_bounds);

  return bubble_bounds;
}

void AnchoredNudge::OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                             views::Widget* widget) const {
  if (set_anchor_view_as_parent_ && GetAnchorView() &&
      GetAnchorView()->GetWidget()) {
    params->parent = GetAnchorView()->GetWidget()->GetNativeView();
    return;
  }

  if (anchor_widget_) {
    params->parent = anchor_widget_->GetNativeView();
    return;
  }

  params->parent = Shell::GetRootWindowForNewWindows()->GetChildById(
      kShellWindowId_SettingBubbleContainer);
}

std::unique_ptr<views::NonClientFrameView>
AnchoredNudge::CreateNonClientFrameView(views::Widget* widget) {
  // Create the customized bubble border.
  std::unique_ptr<views::BubbleBorder> bubble_border =
      std::make_unique<views::BubbleBorder>(arrow(),
                                            views::BubbleBorder::NO_SHADOW);
  bubble_border->set_avoid_shadow_overlap(true);
  bubble_border->set_insets(kBubbleBorderInsets);

  auto frame = BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  static_cast<views::BubbleFrameView*>(frame.get())
      ->SetBubbleBorder(std::move(bubble_border));
  return frame;
}

void AnchoredNudge::AddedToWidget() {
  // Do not attempt fitting the bubble inside the anchor view window.
  GetBubbleFrameView()->set_use_anchor_window_bounds(false);

  // Remove accelerator so the nudge won't be closed when pressing the Esc key.
  GetDialogClientView()->RemoveAccelerator(
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  // Widget needs a native window in order to observe its shelf.
  CHECK(GetWidget()->GetNativeWindow());
  auto* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());

  if (anchored_to_shelf_) {
    DCHECK(GetAnchorView());
    SetArrowFromShelf(shelf);
    disable_shelf_auto_hide_ =
        std::make_unique<Shelf::ScopedDisableAutoHide>(shelf);
    return;
  }

  if (anchor_widget_) {
    // Setting an `anchor_widget_` assumes that there is no anchor view, because
    // widget anchoring is used when an anchor view cannot be set.
    CHECK(!GetAnchorView());

    anchor_widget_scoped_observation_.Observe(anchor_widget_);
    gfx::Point anchor_point =
        GetAnchorPoint(anchor_widget_, anchor_widget_corner_);
    SetAnchorRect(gfx::Rect(anchor_point, gfx::Size(0, 0)));
    return;
  }

  if (!GetAnchorView()) {
    shelf_observation_.Observe(shelf);
    SetDefaultAnchorRect();
  }
}

bool AnchoredNudge::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

bool AnchoredNudge::OnMouseDragged(const ui::MouseEvent& event) {
  return true;
}

void AnchoredNudge::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && !click_callback_.is_null()) {
    std::move(click_callback_).Run();
  }
}

void AnchoredNudge::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureTap: {
      if (!click_callback_.is_null()) {
        std::move(click_callback_).Run();
        event->SetHandled();
      }
      return;
    }
    default: {
      // Do nothing.
    }
  }
}

void AnchoredNudge::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  if (!GetAnchorView()) {
    SetDefaultAnchorRect();
  }
}

void AnchoredNudge::OnHotseatStateChanged(HotseatState old_state,
                                          HotseatState new_state) {
  if (!GetAnchorView()) {
    SetDefaultAnchorRect();
  }
}

void AnchoredNudge::OnShelfAlignmentChanged(aura::Window* root_window,
                                            ShelfAlignment old_alignment) {
  if (!GetWidget() || !GetWidget()->GetNativeWindow()) {
    return;
  }

  // Nudges without an anchor view will be shown on their default location.
  if (!GetAnchorView()) {
    SetDefaultAnchorRect();
    return;
  }

  // Nudges anchored to a view that exists in the shelf need to update their
  // arrow value when the shelf alignment changes.
  if (anchored_to_shelf_) {
    auto* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
    if (shelf == Shelf::ForWindow(root_window)) {
      SetArrowFromShelf(shelf);
    }
  }
}

void AnchoredNudge::OnDisplayMetricsChanged(const display::Display& display,
                                            uint32_t changed_metrics) {
  if (GetAnchorView()) {
    OnAnchorBoundsChanged();
  } else {
    SetDefaultAnchorRect();
  }
}

void AnchoredNudge::OnWidgetDestroying(views::Widget* widget) {
  if (widget != anchor_widget_) {
    return;
  }

  anchor_widget_ = nullptr;
  anchor_widget_scoped_observation_.Reset();
}

void AnchoredNudge::OnWidgetBoundsChanged(views::Widget* widget,
                                          const gfx::Rect& new_bounds) {
  if (widget != anchor_widget_) {
    return;
  }

  gfx::Point anchor_point =
      GetAnchorPoint(anchor_widget_, anchor_widget_corner_);
  SetAnchorRect(gfx::Rect(anchor_point, gfx::Size(0, 0)));
}

void AnchoredNudge::SetArrowFromShelf(Shelf* shelf) {
  if (is_corner_anchored_) {
    SetArrow(shelf->SelectValueForShelfAlignment(
        views::BubbleBorder::BOTTOM_RIGHT, views::BubbleBorder::LEFT_BOTTOM,
        views::BubbleBorder::RIGHT_BOTTOM));
  } else {
    SetArrow(shelf->SelectValueForShelfAlignment(
        views::BubbleBorder::BOTTOM_CENTER, views::BubbleBorder::LEFT_CENTER,
        views::BubbleBorder::RIGHT_CENTER));
  }
}

void AnchoredNudge::SetDefaultAnchorRect() {
  if (anchor_widget_) {
    // The anchor position will be set by tracking the bounds of
    // `anchor_widget_` and update when the widget bounds changed.
    return;
  }

  if (!GetWidget() || !GetWidget()->GetNativeWindow()) {
    return;
  }

  // The default location for a nudge without an `anchor_view` is the leading
  // bottom corner of the work area bounds (bottom-left for LTR languages).
  gfx::Rect work_area_bounds =
      WorkAreaInsets::ForWindow(GetWidget()->GetNativeWindow())
          ->user_work_area_bounds();
  SetAnchorRect(
      gfx::Rect(gfx::Point(base::i18n::IsRTL() ? work_area_bounds.right()
                                               : work_area_bounds.x(),
                           work_area_bounds.bottom()),
                gfx::Size(0, 0)));
}

BEGIN_METADATA(AnchoredNudge)
END_METADATA

}  // namespace ash

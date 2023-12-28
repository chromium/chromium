// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/anchored_nudge.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/toast/system_nudge_view.h"
#include "ash/wm/work_area_insets.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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

constexpr gfx::Insets kBubbleBorderInsets = gfx::Insets(8);

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

}  // namespace

AnchoredNudge::AnchoredNudge(AnchoredNudgeData& nudge_data)
    : views::BubbleDialogDelegateView(nudge_data.GetAnchorView(),
                                      nudge_data.arrow,
                                      views::BubbleBorder::NO_SHADOW),
      id_(nudge_data.id),
      anchored_to_shelf_(nudge_data.anchored_to_shelf),
      click_callback_(std::move(nudge_data.click_callback)),
      dismiss_callback_(std::move(nudge_data.dismiss_callback)) {
  DCHECK(features::IsSystemNudgeV2Enabled());

  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_color(SK_ColorTRANSPARENT);
  set_margins(gfx::Insets());
  set_close_on_deactivate(false);
  SetLayoutManager(std::make_unique<views::FlexLayout>());
  system_nudge_view_ =
      AddChildView(std::make_unique<SystemNudgeView>(nudge_data));

  if (anchored_to_shelf_ || !GetAnchorView()) {
    Shell::Get()->AddShellObserver(this);
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
}

gfx::Rect AnchoredNudge::GetBubbleBounds() {
  auto* root_window = GetWidget()->GetNativeWindow();

  // This can happen during destruction.
  if (!root_window) {
    return gfx::Rect();
  }

  gfx::Rect work_area_bounds =
      WorkAreaInsets::ForWindow(root_window)->user_work_area_bounds();

  auto* hotseat_widget =
      RootWindowController::ForWindow(root_window)->shelf()->hotseat_widget();
  if (hotseat_widget) {
    AdjustWorkAreaBoundsForHotseatState(hotseat_widget, work_area_bounds);
  }

  gfx::Rect bubble_bounds = views::BubbleDialogDelegateView::GetBubbleBounds();
  bubble_bounds.AdjustToFit(work_area_bounds);

  return bubble_bounds;
}

void AnchoredNudge::OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                             views::Widget* widget) const {
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
    case ui::ET_GESTURE_TAP: {
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
  OnAnchorBoundsChanged();
}

void AnchoredNudge::SetArrowFromShelf(Shelf* shelf) {
  SetArrow(shelf->SelectValueForShelfAlignment(
      views::BubbleBorder::BOTTOM_CENTER, views::BubbleBorder::LEFT_CENTER,
      views::BubbleBorder::RIGHT_CENTER));
}

void AnchoredNudge::SetDefaultAnchorRect() {
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

BEGIN_METADATA(AnchoredNudge, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace ash

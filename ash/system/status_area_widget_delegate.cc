// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget_delegate.h"

#include "ash/focus_cycler.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/containers/adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kPaddingBetweenTrayItems = 8;
constexpr int kPaddingBetweenTrayItemsTabletMode = 6;
constexpr int kPaddingBetweenPrimaryTraySetItems = kPaddingBetweenTrayItems - 4;
constexpr int kPaddingBetweenPrimaryTraySetItemsInApp = -4;

class StatusAreaWidgetDelegateAnimationSettings
    : public ui::ScopedLayerAnimationSettings {
 public:
  explicit StatusAreaWidgetDelegateAnimationSettings(ui::Layer* layer)
      : ui::ScopedLayerAnimationSettings(layer->GetAnimator()) {
    SetTransitionDuration(ShelfConfig::Get()->shelf_animation_duration());
    SetPreemptionStrategy(ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    SetTweenType(gfx::Tween::EASE_OUT);
  }

  StatusAreaWidgetDelegateAnimationSettings(
      const StatusAreaWidgetDelegateAnimationSettings&) = delete;
  StatusAreaWidgetDelegateAnimationSettings& operator=(
      const StatusAreaWidgetDelegateAnimationSettings&) = delete;

  ~StatusAreaWidgetDelegateAnimationSettings() override = default;
};

// Gradient background for the status area shown when it overflows into the
// shelf.
class OverflowGradientBackground : public views::Background {
 public:
  explicit OverflowGradientBackground(Shelf* shelf) : shelf_(shelf) {}
  OverflowGradientBackground(const OverflowGradientBackground&) = delete;
  ~OverflowGradientBackground() override = default;
  OverflowGradientBackground& operator=(const OverflowGradientBackground&) =
      delete;

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    gfx::Rect bounds = view->GetContentsBounds();

    SkColor shelf_background_color =
        shelf_->shelf_widget()->GetShelfBackgroundColor();

    cc::PaintFlags flags;
    flags.setShader(gfx::CreateGradientShader(
        gfx::Point(), gfx::Point(kStatusAreaOverflowGradientSize, 0),
        SkColorSetA(shelf_background_color, 0), shelf_background_color));
    canvas->DrawRect(bounds, flags);
  }

 private:
  raw_ptr<Shelf> shelf_;
};

int PaddingBetweenTrayItems(const bool is_in_primary_tray_set) {
  if (is_in_primary_tray_set) {
    // In in-app mode, a negative padding is set. This is because it is set to 6
    // in `TrayContainer`, and this is easier then rewriting TrayContainer to
    // react to `ShelfLayoutManager` state changes. See https://b/310272268.
    return (ShelfConfig::Get()->in_tablet_mode() &&
            ShelfConfig::Get()->is_in_app())
               ? kPaddingBetweenPrimaryTraySetItemsInApp
               : kPaddingBetweenPrimaryTraySetItems;
  }

  if (ShelfConfig::Get()->in_tablet_mode()) {
    return kPaddingBetweenTrayItemsTabletMode;
  }

  return kPaddingBetweenTrayItems;
}

}  // namespace

StatusAreaWidgetDelegate::StatusAreaWidgetDelegate(Shelf* shelf)
    : shelf_(shelf), focus_cycler_for_testing_(nullptr) {
  DCHECK(shelf_);
  SetOwnedByWidget(true);

  // Allow the launcher to surrender the focus to another window upon
  // navigation completion by the user.
  set_allow_deactivate_on_esc(true);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

StatusAreaWidgetDelegate::~StatusAreaWidgetDelegate() = default;

void StatusAreaWidgetDelegate::SetFocusCyclerForTesting(
    const FocusCycler* focus_cycler) {
  focus_cycler_for_testing_ = focus_cycler;
}

bool StatusAreaWidgetDelegate::ShouldFocusOut(bool reverse) {
  views::View* focused_view = GetFocusManager()->GetFocusedView();
  return (reverse && focused_view == GetFirstFocusableChild()) ||
         (!reverse && focused_view == GetLastFocusableChild());
}

void StatusAreaWidgetDelegate::OnStatusAreaCollapseStateChanged(
    StatusAreaWidget::CollapseState new_collapse_state) {
  switch (new_collapse_state) {
    case StatusAreaWidget::CollapseState::EXPANDED:
      SetBackground(std::make_unique<OverflowGradientBackground>(shelf_));
      break;
    case StatusAreaWidget::CollapseState::COLLAPSED:
    case StatusAreaWidget::CollapseState::NOT_COLLAPSIBLE:
      SetBackground(nullptr);
      break;
  }
}

void StatusAreaWidgetDelegate::Shutdown() {
  // TODO(pbos): Investigate if this is necessary. This is a bit defensive but
  // it's done to make sure that StatusAreaWidget isn't accessed by the View
  // hierarchy during its destruction.
  RemoveAllChildViews();
}

void StatusAreaWidgetDelegate::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  AccessiblePaneView::GetAccessibleNodeData(node_data);
  // If OOBE dialog is visible it should be the next accessible widget,
  // otherwise it should be LockScreen.
  if (!!LoginScreen::Get()->GetLoginWindowWidget() &&
      LoginScreen::Get()->GetLoginWindowWidget()->IsVisible()) {
    GetViewAccessibility().SetNextFocus(
        LoginScreen::Get()->GetLoginWindowWidget());
  } else if (LockScreen::HasInstance()) {
    GetViewAccessibility().SetNextFocus(LockScreen::Get()->widget());
  }
  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  GetViewAccessibility().SetPreviousFocus(shelf->shelf_widget());
}

views::View* StatusAreaWidgetDelegate::GetDefaultFocusableChild() {
  return default_last_focusable_child_ ? GetLastFocusableChild()
                                       : GetFirstFocusableChild();
}

views::Widget* StatusAreaWidgetDelegate::GetWidget() {
  return View::GetWidget();
}

const views::Widget* StatusAreaWidgetDelegate::GetWidget() const {
  return View::GetWidget();
}

void StatusAreaWidgetDelegate::OnGestureEvent(ui::GestureEvent* event) {
  views::Widget* target_widget =
      static_cast<views::View*>(event->target())->GetWidget();
  Shelf* shelf = Shelf::ForWindow(target_widget->GetNativeWindow());

  // Convert the event location from current view to screen, since swiping up on
  // the shelf can open the fullscreen app list. Updating the bounds of the app
  // list during dragging is based on screen coordinate space.
  ui::GestureEvent event_in_screen(*event);
  gfx::Point location_in_screen(event->location());
  View::ConvertPointToScreen(this, &location_in_screen);
  event_in_screen.set_location(location_in_screen);
  if (shelf->ProcessGestureEvent(event_in_screen))
    event->StopPropagation();
  else
    views::AccessiblePaneView::OnGestureEvent(event);
}

bool StatusAreaWidgetDelegate::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  const FocusCycler* focus_cycler = focus_cycler_for_testing_
                                        ? focus_cycler_for_testing_.get()
                                        : Shell::Get()->focus_cycler();
  return focus_cycler->widget_activating() == GetWidget();
}

void StatusAreaWidgetDelegate::CalculateTargetBounds() {
  const auto it =
      base::ranges::find(base::Reversed(children()), true, &View::GetVisible);
  const View* last_visible_child = it == children().crend() ? nullptr : *it;

  // Set the border for each child, with a different border for the edge child.
  for (views::View* child : children()) {
    if (!child->GetVisible())
      continue;
    SetBorderOnChild(child, last_visible_child == child);
  }

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->SetOrientation(shelf_->IsHorizontalAlignment()
                             ? views::BoxLayout::Orientation::kHorizontal
                             : views::BoxLayout::Orientation::kVertical);

  target_bounds_.set_size(GetPreferredSize());
}

gfx::Rect StatusAreaWidgetDelegate::GetTargetBounds() const {
  return target_bounds_;
}

void StatusAreaWidgetDelegate::UpdateLayout(bool animate) {
  if (animate) {
    StatusAreaWidgetDelegateAnimationSettings settings(layer());
    DeprecatedLayoutImmediately();
  } else {
    DeprecatedLayoutImmediately();
  }
}

void StatusAreaWidgetDelegate::ChildPreferredSizeChanged(View* child) {
  const gfx::Size current_size = size();
  const gfx::Size new_size = GetPreferredSize();
  if (new_size == current_size)
    return;
  // Need to re-layout the shelf when trays or items are added/removed.
  // don't run uring login or unlock if the shelf container is animating.
  std::unique_ptr<StatusAreaWidgetDelegateAnimationSettings> settings;
  if (!shelf_->shelf_widget()
           ->GetNativeWindow()
           ->parent()
           ->layer()
           ->GetAnimator()
           ->is_animating()) {
    settings =
        std::make_unique<StatusAreaWidgetDelegateAnimationSettings>(layer());
  }
  shelf_->shelf_layout_manager()->LayoutShelf(/*animate=*/false);
}

void StatusAreaWidgetDelegate::ChildVisibilityChanged(View* child) {
  shelf_->shelf_layout_manager()->LayoutShelf(/*animate=*/true);
}

void StatusAreaWidgetDelegate::SetBorderOnChild(views::View* child,
                                                bool is_child_on_edge) {
  // TODO(https://b/310272268): Setting padding both here and in `TrayContainer`
  // is a bit confusing. This is fragile, and we should rewrite this.
  const int vertical_padding =
      (ShelfConfig::Get()->shelf_size() - kTrayItemSize) / 2;

  // Edges for horizontal alignment (right-to-left, default).
  int top_edge = vertical_padding;
  int left_edge = 0;
  int bottom_edge = vertical_padding;

  // Add some extra space so that borders don't overlap. This padding between
  // items also takes care of padding at the edge of the shelf.
  int right_edge;
  if (is_child_on_edge) {
    right_edge = ShelfConfig::Get()->control_button_edge_spacing(
        true /* is_primary_axis_edge */);
  } else {
    // The primary tray set contains the notification tray, the date tray and
    // the status tray. The status tray is always on the edge, so that case is
    // covered in the `if` condition.
    const bool is_in_primary_tray_set =
        child->GetID() == VIEW_ID_SA_DATE_TRAY ||
        child->GetID() == VIEW_ID_SA_NOTIFICATION_TRAY;

    right_edge = PaddingBetweenTrayItems(is_in_primary_tray_set);
  }

  // Swap edges if alignment is not horizontal (bottom-to-top).
  if (!shelf_->IsHorizontalAlignment()) {
    std::swap(top_edge, left_edge);
    std::swap(bottom_edge, right_edge);
  }

  child->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(top_edge, left_edge, bottom_edge, right_edge)));

  // Layout on |child| needs to be updated based on new border value before
  // displaying; otherwise |child| will be showing with old border size.
  // Fix for crbug.com/623438.
  child->DeprecatedLayoutImmediately();
}

BEGIN_METADATA(StatusAreaWidgetDelegate)
END_METADATA

}  // namespace ash

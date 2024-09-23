// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/desk_button_widget.h"

#include "ash/focus_cycler.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk_button/desk_button_container.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/i18n/rtl.h"
#include "base/ranges/algorithm.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace {

gfx::Point GetScreenLocationForEvent(aura::Window* root,
                                     const ui::LocatedEvent& event) {
  gfx::Point screen_location;
  if (event.target()) {
    screen_location = event.target()->GetScreenLocation(event);
  } else {
    screen_location = event.root_location();
    wm::ConvertPointToScreen(root, &screen_location);
  }
  return screen_location;
}

}  // namespace

namespace ash {

// Customized window targeter that lets events fall through to the shelf if they
// do not intersect with desk button UIs.
class DeskButtonWindowTargeter : public aura::WindowTargeter {
 public:
  explicit DeskButtonWindowTargeter(DeskButtonWidget* desk_button_widget)
      : desk_button_widget_(desk_button_widget) {}
  DeskButtonWindowTargeter(const DeskButtonWindowTargeter&) = delete;
  DeskButtonWindowTargeter& operator=(const DeskButtonWindowTargeter&) = delete;

  // aura::WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(aura::Window* window,
                                       const ui::LocatedEvent& event) override {
    // Convert to screen coordinate. Do not process the event if it's not on the
    // delegate view.
    const gfx::Point screen_location =
        GetScreenLocationForEvent(window->GetRootWindow(), event);
    const gfx::Rect screen_bounds =
        desk_button_widget_->delegate_view()->GetBoundsInScreen();
    if (!screen_bounds.Contains(screen_location)) {
      return false;
    }

    // Process the event if it intersects with desk button UI, otherwise let the
    // event fall through to the shelf.
    return desk_button_widget_->GetDeskButtonContainer()
        ->IntersectsWithDeskButtonUi(screen_location);
  }

 private:
  const raw_ptr<DeskButtonWidget> desk_button_widget_;
};

DeskButtonWidget::DelegateView::DelegateView() = default;
DeskButtonWidget::DelegateView::~DelegateView() = default;

void DeskButtonWidget::DelegateView::Init(
    DeskButtonWidget* desk_button_widget) {
  CHECK(desk_button_widget);
  desk_button_widget_ = desk_button_widget;
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  GetContentsView()->AddChildView(views::Builder<DeskButtonContainer>()
                                      .CopyAddressTo(&desk_button_container_)
                                      .Init(desk_button_widget_)
                                      .Build());
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

bool DeskButtonWidget::DelegateView::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  return Shell::Get()->focus_cycler()->widget_activating() == GetWidget();
}

void DeskButtonWidget::DelegateView::Layout(PassKey) {
  if (!desk_button_widget_ || !desk_button_container_) {
    return;
  }

  // Update the desk button container.
  desk_button_container_->set_zero_state(
      !desk_button_widget_->IsHorizontalShelf());
  desk_button_container_->UpdateUi(DesksController::Get()->active_desk());

  // Calculate bounds of the desk button container.
  const gfx::Size widget_size =
      desk_button_widget_->GetWindowBoundsInScreen().size();
  const gfx::Size container_size = desk_button_container_->GetPreferredSize();
  gfx::Point container_origin;
  if (desk_button_widget_->IsHorizontalShelf()) {
    container_origin = gfx::Point(
        widget_size.width() - kDeskButtonWidgetInsetsHorizontal.right() -
            container_size.width(),
        kDeskButtonWidgetInsetsHorizontal.top());
  } else {
    container_origin = gfx::Point(kDeskButtonWidgetInsetsVertical.left(),
                                  widget_size.height() -
                                      kDeskButtonWidgetInsetsVertical.bottom() -
                                      container_size.height());
  }

  desk_button_container_->SetBoundsRect({container_origin, container_size});
}

bool DeskButtonWidget::DelegateView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  CHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  GetWidget()->Deactivate();
  return true;
}

DeskButtonWidget::DeskButtonWidget(Shelf* shelf) : shelf_(shelf) {
  CHECK(shelf_);
}

DeskButtonWidget::~DeskButtonWidget() = default;

// static
int DeskButtonWidget::GetMaxLength(bool horizontal_shelf) {
  const int container_len =
      DeskButtonContainer::GetMaxLength(!horizontal_shelf);
  return container_len + (horizontal_shelf
                              ? kDeskButtonWidgetInsetsHorizontal.width()
                              : kDeskButtonWidgetInsetsVertical.height());
}

bool DeskButtonWidget::ShouldReserveSpaceFromShelf() const {
  const ShelfLayoutManager* layout_manager = shelf_->shelf_layout_manager();
  Shell* shell = Shell::Get();
  PrefService* prefs =
      shell->session_controller()->GetLastActiveUserPrefService();
  return layout_manager->is_active_session_state() &&
         !shell->IsInTabletMode() && prefs && GetDeskButtonVisibility(prefs);
}

bool DeskButtonWidget::ShouldBeVisible() const {
  const OverviewController* overview_controller =
      Shell::Get()->overview_controller();
  return ShouldReserveSpaceFromShelf() &&
         !overview_controller->InOverviewSession();
}

void DeskButtonWidget::PrepareForAlignmentChange() {
  delegate_view_->desk_button_container()->PrepareForAlignmentChange();
}

void DeskButtonWidget::CalculateTargetBounds() {
  if (!ShouldBeVisible()) {
    target_bounds_ = gfx::Rect();
    return;
  }

  gfx::Point widget_origin;
  gfx::Size widget_size;

  // The position of this widget is always dependant on the hotseat widget.
  const gfx::Rect hotseat_bounds = shelf_->hotseat_widget()->GetTargetBounds();
  const gfx::Insets shelf_padding =
      shelf_->hotseat_widget()
          ->scrollable_shelf_view()
          ->CalculateMirroredEdgePadding(/*use_target_bounds=*/true);
  const int app_icon_end_padding = ShelfConfig::Get()->GetAppIconEndPadding();
  const int max_length = GetMaxLength(IsHorizontalShelf());

  if (IsHorizontalShelf()) {
    widget_size = gfx::Size(max_length, hotseat_bounds.height());
    widget_origin = gfx::Point(
        base::i18n::IsRTL() ? hotseat_bounds.right() - shelf_padding.right() -
                                  app_icon_end_padding
                            : hotseat_bounds.x() + shelf_padding.left() +
                                  app_icon_end_padding - widget_size.width(),
        hotseat_bounds.y());
  } else {
    widget_size = gfx::Size(hotseat_bounds.width(), max_length);
    widget_origin = gfx::Point(hotseat_bounds.x(),
                               hotseat_bounds.y() + shelf_padding.top() +
                                   app_icon_end_padding - widget_size.height());
  }

  target_bounds_ = gfx::Rect(widget_origin, widget_size);
}

gfx::Rect DeskButtonWidget::GetTargetBounds() const {
  return target_bounds_;
}

void DeskButtonWidget::UpdateLayout(bool animate) {
  const gfx::Rect initial_bounds = GetWindowBoundsInScreen();
  const bool visibility = GetVisible();
  const bool target_visibility = ShouldBeVisible();
  if (initial_bounds == target_bounds_ && visibility == target_visibility) {
    return;
  }

  if (!animate || visibility != target_visibility || initial_bounds.IsEmpty() ||
      target_bounds_.IsEmpty()) {
    if (target_visibility && !target_bounds_.IsEmpty()) {
      SetBounds(target_bounds_);
      ShowInactive();
    } else {
      Hide();
    }

    return;
  }

  // We only animate x axis movement for bottom shelf and y axis movement for
  // side shelf when the widget size remains the same and non empty.
  const bool animate_transform =
      initial_bounds.size() == target_bounds_.size() &&
      !target_bounds_.IsEmpty() &&
      ((IsHorizontalShelf() && initial_bounds.y() == target_bounds_.y()) ||
       (!IsHorizontalShelf() && initial_bounds.x() == target_bounds_.x()));

  if (animate_transform) {
    const gfx::Transform initial_transform = gfx::TransformBetweenRects(
        gfx::RectF(target_bounds_), gfx::RectF(initial_bounds));
    SetBounds(target_bounds_);
    GetNativeView()->layer()->SetTransform(initial_transform);
  }

  ui::ScopedLayerAnimationSettings animation_setter(
      GetNativeView()->layer()->GetAnimator());
  animation_setter.SetTransitionDuration(
      ShelfConfig::Get()->shelf_animation_duration());
  animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
  animation_setter.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  if (animate_transform) {
    GetNativeView()->layer()->SetTransform(gfx::Transform());
  } else {
    SetBounds(target_bounds_);
  }
}

void DeskButtonWidget::UpdateTargetBoundsForGesture(int shelf_position) {
  if (IsHorizontalShelf()) {
    target_bounds_.set_y(shelf_position);
  } else {
    target_bounds_.set_x(shelf_position);
  }
}

void DeskButtonWidget::HandleLocaleChange() {
  delegate_view_->desk_button_container()->HandleLocaleChange();
}

void DeskButtonWidget::Initialize(aura::Window* container) {
  CHECK(container);
  delegate_view_ = new DelegateView();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "DeskButtonWidget";
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.delegate = delegate_view_;
  params.parent = container;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  Init(std::move(params));
  set_focus_on_creation(false);
  delegate_view_->SetEnableArrowKeyTraversal(true);

  delegate_view_->Init(this);

  CalculateTargetBounds();
  UpdateLayout(/*animate=*/false);

  GetNativeWindow()->SetEventTargeter(
      std::make_unique<DeskButtonWindowTargeter>(/*desk_button_widget=*/this));
}

DeskButtonContainer* DeskButtonWidget::GetDeskButtonContainer() const {
  return delegate_view_->desk_button_container();
}

bool DeskButtonWidget::IsHorizontalShelf() const {
  return shelf_->IsHorizontalAlignment();
}

void DeskButtonWidget::SetDefaultChildToFocus(
    views::View* default_child_to_focus) {
  CHECK(!default_child_to_focus || (default_child_to_focus->GetVisible() &&
                                    default_child_to_focus->GetEnabled()));
  default_child_to_focus_ = default_child_to_focus;
}

void DeskButtonWidget::StoreDeskButtonFocus() {
  stored_focused_view_ = ShouldBeVisible() && IsActive()
                             ? GetFocusManager()->GetFocusedView()
                             : nullptr;
  CHECK(!stored_focused_view_ || (stored_focused_view_->GetVisible() &&
                                  stored_focused_view_->GetEnabled()));
}

void DeskButtonWidget::RestoreDeskButtonFocus() {
  if (ShouldBeVisible() && stored_focused_view_) {
    default_child_to_focus_ = stored_focused_view_;
    stored_focused_view_ = nullptr;
    Shell::Get()->focus_cycler()->FocusWidget(this);
  }
}

void DeskButtonWidget::MaybeFocusOut(bool reverse) {
  // Only focus visible and enabled views.
  std::vector<views::View*> views;
  for (auto view : GetDeskButtonContainer()->children()) {
    if (view->GetVisible() && view->GetEnabled()) {
      views.emplace_back(view);
    }
  }

  // The desk button will still be drawn in LTR, with the previous desk button
  // on the left, when in RTL mode.
  if (base::i18n::IsRTL()) {
    base::ranges::reverse(views);
  }

  views::View* focused_view = GetFocusManager()->GetFocusedView();
  const int count = views.size();
  int focused = base::ranges::find(views, focused_view) - std::begin(views);
  if (focused == count) {
    GetFocusManager()
        ->GetNextFocusableView(nullptr, nullptr, !reverse, false)
        ->RequestFocus();
    return;
  }

  int next = focused + (reverse ? -1 : 1);
  if (next < 0 || next >= count) {
    shelf_->shelf_focus_cycler()->FocusOut(reverse, SourceView::kDeskButton);
    return;
  }
  views[next]->RequestFocus();
}

bool DeskButtonWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active)) {
    return false;
  }

  if (active && default_child_to_focus_) {
    default_child_to_focus_->RequestFocus();
    default_child_to_focus_ = nullptr;
  }

  return true;
}

}  // namespace ash

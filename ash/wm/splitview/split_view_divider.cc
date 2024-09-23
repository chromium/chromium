// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_divider.h"

#include <algorithm>
#include <memory>

#include "ash/display/screen_orientation_controller.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider_view.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "ui/aura/window_targeter.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Inset value for the transient parent, ensuring the divider remains visible
// and clear of the window resizer border.
constexpr int kTransientParentInset = chromeos::kResizeOutsideBoundsSize + 1;

// Returns the allowed range of `divider_position` within `windows`,
// accounting for the windows' minimum sizes.
gfx::Range GetDividerPositionAllowedRange(const aura::Window::Windows windows) {
  CHECK(!windows.empty());

  aura::Window* root_window = windows.at(0)->GetRootWindow();
  aura::Window* primary_window = nullptr;
  aura::Window* secondary_window = nullptr;
  for (auto window : windows) {
    if (IsPhysicallyLeftOrTop(window)) {
      primary_window = window;
    } else {
      secondary_window = window;
    }
  }
  CHECK(primary_window || secondary_window);

  const bool is_horizontal = IsLayoutHorizontal(root_window);
  const int primary_window_minimum_length =
      GetMinimumWindowLength(primary_window, is_horizontal);
  const int secondary_window_minimum_length =
      GetMinimumWindowLength(secondary_window, is_horizontal);
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window);
  return gfx::Range(primary_window_minimum_length,
                    is_horizontal ? (work_area_bounds_in_screen.width() -
                                     secondary_window_minimum_length -
                                     kSplitviewDividerShortSideLength)
                                  : (work_area_bounds_in_screen.height() -
                                     secondary_window_minimum_length -
                                     kSplitviewDividerShortSideLength));
}

gfx::Point GetBoundedPosition(const gfx::Point& location_in_screen,
                              const gfx::Rect& bounds_in_screen) {
  return gfx::Point(std::clamp(location_in_screen.x(), bounds_in_screen.x(),
                               bounds_in_screen.right() - 1),
                    std::clamp(location_in_screen.y(), bounds_in_screen.y(),
                               bounds_in_screen.bottom() - 1));
}

gfx::Rect GetWorkAreaBoundsInScreen(aura::Window* window) {
  return screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
      window);
}

// Returns the widget init params needed to create the widget.
views::Widget::InitParams CreateWidgetInitParams(aura::Window* parent_window,
                                                 const gfx::Rect& bounds) {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.parent = parent_window;
  params.bounds = bounds;
  params.init_properties_container.SetProperty(kExcludeInMruKey, true);
  params.init_properties_container.SetProperty(kIgnoreWindowActivationKey,
                                               true);
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  // Exclude the divider from getting transformed with its transient parent
  // window when we are resizing. The divider will set its own transforms.
  params.init_properties_container.SetProperty(
      kExcludeFromTransientTreeTransformKey, true);
  params.name = "SplitViewDividerWidget";
  return params;
}

}  // namespace

// SplitViewDividerWidget observes its native widget activation change to set
// pane focus on its contents view.
class SplitViewDivider::SplitViewDividerWidget : public views::Widget {
 public:
  SplitViewDividerWidget() = default;
  SplitViewDividerWidget(const SplitViewDividerWidget&) = delete;
  SplitViewDividerWidget& operator=(const SplitViewDividerWidget&) = delete;
  ~SplitViewDividerWidget() override = default;

  // views::Widget:
  bool OnNativeWidgetActivationChanged(bool active) override {
    if (!Widget::OnNativeWidgetActivationChanged(active)) {
      return false;
    }
    // Only set focus and show the focus ring if `this` is being activated by
    // the focus cycler.
    if (!active || this != Shell::Get()->focus_cycler()->widget_activating()) {
      return false;
    }
    base::RecordAction(
        base::UserMetricsAction("SnapGroups_ActivateViaKeyboard"));
    auto* divider_view =
        views::AsViewClass<SplitViewDividerView>(GetContentsView());
    divider_view->SetPaneFocusAndFocusDefault();
    return true;
  }

  // ui::ColorProviderSource:
  ui::ColorProviderKey GetColorProviderKey() const override {
    //  As the transient child of the topmost window, divider uses that window's
    //  theme color. Override `GetColorProviderKey()` to let it use the system's
    //  theme instead.
    return ui::NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(
        nullptr);
  }
};

SplitViewDivider::SplitViewDivider(LayoutDividerController* controller)
    : controller_(controller) {}

SplitViewDivider::~SplitViewDivider() = default;

// static
gfx::Rect SplitViewDivider::GetDividerBoundsInScreen(
    const gfx::Rect& work_area_bounds_in_screen,
    bool landscape,
    int divider_position,
    bool is_dragging) {
  const int dragging_diff = is_dragging
                                ? (kSplitviewDividerEnlargedShortSideLength -
                                   kSplitviewDividerShortSideLength) /
                                      2
                                : 0;
  if (landscape) {
    return gfx::Rect(
        work_area_bounds_in_screen.x() + divider_position - dragging_diff,
        work_area_bounds_in_screen.y(),
        is_dragging ? kSplitviewDividerEnlargedShortSideLength
                    : kSplitviewDividerShortSideLength,
        work_area_bounds_in_screen.height());
  } else {
    return gfx::Rect(
        work_area_bounds_in_screen.x(),
        work_area_bounds_in_screen.y() + divider_position - dragging_diff,
        work_area_bounds_in_screen.width(),
        is_dragging ? kSplitviewDividerEnlargedShortSideLength
                    : kSplitviewDividerShortSideLength);
  }
}

aura::Window* SplitViewDivider::GetDividerWindow() {
  return divider_widget_ ? divider_widget_->GetNativeWindow() : nullptr;
}

bool SplitViewDivider::HasDividerWidget() const {
  return !!divider_widget_;
}

bool SplitViewDivider::IsDividerWidgetVisible() const {
  return divider_widget_ && divider_widget_->IsVisible();
}

void SplitViewDivider::SetVisible(bool visible) {
  if (target_visibility_ != visible) {
    target_visibility_ = visible;
    RefreshDividerState(/*observed_windows_changed=*/false);
  }
}

void SplitViewDivider::SetDividerPosition(int divider_position) {
  if (divider_position_ == divider_position) {
    return;
  }
  divider_position_ = divider_position;
  // Only clamp within `observed_windows_` if it is not empty; otherwise it
  // will return an invalid range.
  // TODO(michelefan): Fix tablet mode regression: when the divider is dragged
  // below the minimum window size, slide the window out to prevent errors.
  if (!observed_windows_.empty() &&
      !display::Screen::GetScreen()->InTabletMode()) {
    const gfx::Range divider_allowed_range =
        GetDividerPositionAllowedRange(observed_windows_);
    if (!divider_allowed_range.is_reversed()) {
      divider_position_ = std::clamp(
          divider_position_, static_cast<int>(divider_allowed_range.start()),
          static_cast<int>(divider_allowed_range.end()));
    }
    // TODO(b/333623218): If the divider range is reversed, i.e. the windows no
    // longer fit, we will break the group.
  }
  RefreshDividerState(/*observed_windows_changed=*/false);
}

void SplitViewDivider::UpdateDividerPosition(
    const gfx::Point& location_in_screen) {
  aura::Window* root = GetRootWindow();
  const bool horizontal = IsLayoutHorizontal(root);
  if (!display::Screen::GetScreen()->InTabletMode()) {
    // In clamshell mode, we try to keep the center point of the divider as in
    // sync with the mouse event location as possible. `SetDividerPosition()`
    // will clamp the position between the windows' minimum sizes.
    gfx::Point location_in_root(location_in_screen);
    wm::ConvertPointFromScreen(root, &location_in_root);
    // Note `divider_position` needs to be relative to the work area to get the
    // correct bounds in `GetDividerBoundsInScreen()`.
    gfx::Rect work_area = GetWorkAreaBoundsInScreen(root);
    wm::ConvertRectFromScreen(root, &work_area);
    SetDividerPosition(
        horizontal ? location_in_root.x() -
                         kSplitviewDividerShortSideLength / 2 - work_area.x()
                   : location_in_root.y() -
                         kSplitviewDividerShortSideLength / 2 - work_area.y());
    return;
  }

  int potential_divider_position = divider_position_;
  if (horizontal) {
    potential_divider_position +=
        location_in_screen.x() - previous_event_location_.x();
  } else {
    potential_divider_position +=
        location_in_screen.y() - previous_event_location_.y();
  }

  // This line is used for ARC++ windows.
  potential_divider_position = std::max(0, potential_divider_position);

  SetDividerPosition(potential_divider_position);
}

aura::Window* SplitViewDivider::GetRootWindow() const {
  return controller_->GetRootWindow();
}

void SplitViewDivider::StartResizeWithDivider(
    const gfx::Point& location_in_screen) {
  // `is_resizing_with_divider_` may be true here, because you can start
  // dragging the divider with a pointing device while already dragging it by
  // touch, or vice versa. It is possible by using the emulator or
  // chrome://flags/#force-tablet-mode. Bailing out here does not stop the user
  // from dragging by touch and with a pointing device simultaneously; it just
  // avoids duplicate calls to `CreateDragDetails()` and `OnDragStarted()`. We
  // also bail out here if you try to start dragging the divider during its snap
  // animation.
  // TODO(sophiewen): Consider refactoring `DividerSnapAnimation` to here.
  if (is_resizing_with_divider_ ||
      SplitViewController::Get(GetRootWindow())->IsDividerAnimating()) {
    return;
  }

  is_resizing_with_divider_ = true;
  EnlargeOrShrinkDivider(/*should_enlarge=*/true);
  previous_event_location_ = location_in_screen;

  UpdateDividerPosition(location_in_screen);
  controller_->StartResizeWithDivider(location_in_screen);

  for (aura::Window* window : observed_windows_) {
    if (window == nullptr) {
      continue;
    }

    WindowState* window_state = WindowState::Get(window);
    gfx::Point location_in_parent(location_in_screen);
    wm::ConvertPointFromScreen(window->parent(), &location_in_parent);
    int window_component = GetWindowComponentForResize(window);
    window_state->CreateDragDetails(gfx::PointF(location_in_parent),
                                    window_component,
                                    wm::WINDOW_MOVE_SOURCE_TOUCH);

    window_state->OnDragStarted(window_component);
  }
}

void SplitViewDivider::ResizeWithDivider(const gfx::Point& location_in_screen) {
  if (!is_resizing_with_divider_) {
    return;
  }

  base::AutoReset<bool> auto_reset(&processing_resize_event_, true);

  gfx::Point modified_location_in_screen = GetBoundedPosition(
      location_in_screen,
      GetWorkAreaBoundsInScreen(divider_widget_->GetNativeWindow()));

  // Order here matters: we first update `divider_position_`, then the
  // `LayoutDividerController` will update the window and divider bounds in
  // `UpdateResizeWithDivider()`.
  UpdateDividerPosition(modified_location_in_screen);
  EnlargeOrShrinkDivider(/*should_enlarge=*/true);
  controller_->UpdateResizeWithDivider(modified_location_in_screen);

  previous_event_location_ = modified_location_in_screen;
}

void SplitViewDivider::EndResizeWithDivider(
    const gfx::Point& location_in_screen) {
  if (!is_resizing_with_divider_) {
    return;
  }

  is_resizing_with_divider_ = false;

  gfx::Point modified_location_in_screen = GetBoundedPosition(
      location_in_screen, GetWorkAreaBoundsInScreen(GetRootWindow()));

  // Order here matters: we first update `divider_position_`, then the
  // `LayoutDividerController` will transform and update the windows bounds in
  // `EndResizeWithDivider()`.
  UpdateDividerPosition(modified_location_in_screen);
  const gfx::Point cursor_point =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  EnlargeOrShrinkDivider(
      GetDividerBoundsInScreen(/*is_dragging=*/true).Contains(cursor_point));

  // If the delegate is done with resizing, finish resizing and clean up.
  // Otherwise it will be called later, in
  // `DividerSnapAnimation::AnimationEnded()`.
  if (controller_->EndResizeWithDivider(modified_location_in_screen)) {
    CleanUpWindowResizing();
  }
}

void SplitViewDivider::CleanUpWindowResizing() {
  is_resizing_with_divider_ = false;
  // Always call `OnResizeEnding()` since `CleanUpWindowResizing()` may be after
  // an animation and we need to restore the window transforms.
  controller_->OnResizeEnding();
  FinishWindowResizing();
  controller_->OnResizeEnded();
}

void SplitViewDivider::UpdateDividerBounds() {
  if (divider_widget_) {
    divider_widget_->SetBounds(GetDividerBoundsInScreen(/*is_dragging=*/false));
  }
}

gfx::Rect SplitViewDivider::GetDividerBoundsInScreen(bool is_dragging) {
  auto* root_window = GetRootWindow();
  const gfx::Rect work_area_bounds_in_screen =
      GetWorkAreaBoundsInScreen(root_window);
  return GetDividerBoundsInScreen(work_area_bounds_in_screen,
                                  IsLayoutHorizontal(root_window),
                                  divider_position_, is_dragging);
}

void SplitViewDivider::EnlargeOrShrinkDivider(bool should_enlarge) {
  if (!divider_widget_ || !divider_widget_->IsVisible()) {
    return;
  }

  divider_widget_->SetBounds(GetDividerBoundsInScreen(should_enlarge));
  divider_view_->RefreshDividerHandler();

  // Even though the divider is a transient of the topmost window, it's not
  // observed. Mouse/gesture events on the divider may not trigger a refresh of
  // the stacking order which becomes noticeable with the existence of other
  // observed transient windows (divider stacked on top of the transient
  // window). Explicitly call `RefreshStackingOrder()` to apply needed
  // adjustments.
  RefreshStackingOrder();
}

void SplitViewDivider::SetAdjustable(bool adjustable) {
  if (adjustable == IsAdjustable()) {
    return;
  }

  divider_widget_->GetNativeWindow()->SetEventTargetingPolicy(
      adjustable ? aura::EventTargetingPolicy::kTargetAndDescendants
                 : aura::EventTargetingPolicy::kNone);
  divider_view_->SetHandlerBarVisible(adjustable);
}

bool SplitViewDivider::IsAdjustable() const {
  DCHECK(divider_widget_);
  DCHECK(divider_widget_->GetNativeView());
  return divider_widget_->GetNativeWindow()->event_targeting_policy() !=
         aura::EventTargetingPolicy::kNone;
}

void SplitViewDivider::MaybeAddObservedWindow(aura::Window* window) {
  if (base::Contains(observed_windows_, window)) {
    return;
  }
  window->AddObserver(this);
  observed_windows_.push_back(window);
  wm::TransientWindowManager* transient_manager =
      wm::TransientWindowManager::GetOrCreate(window);
  transient_manager->AddObserver(this);
  for (aura::Window* transient_window :
       transient_manager->transient_children()) {
    StartObservingTransientChild(transient_window);
  }

  RefreshDividerState(/*observed_windows_changed=*/true);
}

void SplitViewDivider::MaybeRemoveObservedWindow(aura::Window* window) {
  auto iter = base::ranges::find(observed_windows_, window);
  if (iter != observed_windows_.end()) {
    window->RemoveObserver(this);
    observed_windows_.erase(iter);
    wm::TransientWindowManager* transient_manager =
        wm::TransientWindowManager::GetOrCreate(window);
    transient_manager->RemoveObserver(this);
    for (aura::Window* transient_window :
         transient_manager->transient_children()) {
      StopObservingTransientChild(transient_window);
    }
    RefreshDividerState(/*observed_windows_changed=*/true);
  }
}

void SplitViewDivider::OnKeyboardOccludedBoundsChangedInPortrait(
    const gfx::Rect& work_area,
    int y) {
  // If the divider widget doesn't exist, i.e. in clamshell split view, we are
  // done.
  if (!divider_widget_) {
    return;
  }

  CHECK(!IsLayoutHorizontal(GetRootWindow()));

  // Else subtract the divider width and update the widget bounds. Note we
  // *don't* update `divider_position_` since it may be used to restore the
  // window bounds in `SplitViewController::OnWindowActivated()`.
  // TODO(b/331459348): Investigate why we don't update `divider_position_` and
  // fix this code.
  const int divider_position = y - kSplitviewDividerShortSideLength;
  divider_widget_->SetBounds(
      GetDividerBoundsInScreen(work_area, /*landscape=*/false, divider_position,
                               /*is_dragging=*/false));

  // Make split view divider unadjustable.
  SetAdjustable(false);
}

void SplitViewDivider::OnWindowDragStarted(aura::Window* dragged_window) {
  dragged_window_ = dragged_window;
  RefreshStackingOrder();
}

void SplitViewDivider::OnWindowDragEnded() {
  dragged_window_ = nullptr;
  RefreshStackingOrder();
}

void SplitViewDivider::SwapWindows() {
  controller_->SwapWindows();
}

void SplitViewDivider::OnWindowDestroying(aura::Window* window) {
  MaybeRemoveObservedWindow(window);
}

void SplitViewDivider::OnWindowBoundsChanged(aura::Window* window,
                                             const gfx::Rect& old_bounds,
                                             const gfx::Rect& new_bounds,
                                             ui::PropertyChangeReason reason) {
  if (is_resizing_with_divider_ &&
      display::Screen::GetScreen()->InTabletMode() &&
      base::Contains(observed_windows_, window)) {
    // Bounds may be changed while we are processing a resize event. In this
    // case, we don't update the windows transform here, since it will be done
    // soon anyway. If we are *not* currently processing a resize, it means the
    // bounds of a window have been updated "async", and we need to update the
    // window's transform.
    if (!processing_resize_event_) {
      // TODO(b/308819668): Remove this reference to `SplitViewController` when
      // we move `divider_position` to here.
      const int divider_position =
          SplitViewController::Get(GetRootWindow())->GetDividerPosition();
      for (aura::Window* window_to_transform : observed_windows_) {
        SetWindowTransformDuringResizing(window_to_transform, divider_position);
      }
    }
  }

  // We only care about the bounds change of windows in
  // |transient_windows_observations_|.
  if (!transient_windows_observations_.IsObservingSource(window))
    return;

  // |window|'s transient parent must be one of the windows in
  // |observed_windows_|.
  aura::Window* transient_parent = nullptr;
  for (aura::Window* observed_window : observed_windows_) {
    if (wm::HasTransientAncestor(window, observed_window)) {
      transient_parent = observed_window;
      break;
    }
  }
  DCHECK(transient_parent);

  // Inset the bounds of the `transient_parent` by `kTransientParentInset`
  // to prevent the snapped window's resize border from obscuring the divider.
  // This simplifies resizing when a transient window is present.
  gfx::Rect adjusted_transient_parent_bounds =
      transient_parent->GetBoundsInScreen();
  adjusted_transient_parent_bounds.Inset(gfx::Insets(kTransientParentInset));
  gfx::Rect transient_bounds = window->GetBoundsInScreen();
  transient_bounds.AdjustToFit(adjusted_transient_parent_bounds);

  window->SetBoundsInScreen(
      transient_bounds,
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
}

void SplitViewDivider::OnWindowStackingChanged(aura::Window* window) {
  RefreshStackingOrder();
}

void SplitViewDivider::OnWindowVisibilityChanged(aura::Window* window,
                                                 bool visible) {
  RefreshStackingOrder();
}

void SplitViewDivider::OnTransientChildAdded(aura::Window* window,
                                             aura::Window* transient) {
  StartObservingTransientChild(transient);
}

void SplitViewDivider::OnTransientChildRemoved(aura::Window* window,
                                               aura::Window* transient) {
  StopObservingTransientChild(transient);
}

void SplitViewDivider::OnDisplayMetricsChanged(const display::Display& display,
                                               uint32_t metrics) {
  if (!(metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION |
         DISPLAY_METRIC_DEVICE_SCALE_FACTOR | DISPLAY_METRIC_WORK_AREA))) {
    return;
  }

  UpdateDividerBounds();
}

void SplitViewDivider::RefreshDividerState(bool observed_windows_changed) {
  // Avoid any recursive updates.
  if (is_refreshing_state_) {
    return;
  }
  base::AutoReset<bool> lock(&is_refreshing_state_, true);

  if (observed_windows_.empty()) {
    if (divider_widget_) {
      CloseDividerWidget();
    }
    return;
  }

  if (observed_windows_changed) {
    // Re-set the position since a new set of observed windows might mean
    // different allowed range for the divider position.
    SetDividerPosition(divider_position_);
  }

  // Stacking order is refreshed only if the widget has just been created or the
  // observed windows have changed.
  bool refresh_stacking_order = observed_windows_changed;
  if (!divider_widget_) {
    CreateDividerWidget(divider_position_);
    refresh_stacking_order = true;
  }

  const bool update_visibility =
      target_visibility_ != GetActualTargetVisibility();

  if (target_visibility_) {
    UpdateDividerBounds();
    if (update_visibility) {
      // Call `ShowInactive()` to avoid an unnecessary window activation change
      // when the divider is shown or hidden.
      divider_widget_->ShowInactive();
      // Since the divider may be hidden and re-shown during
      // `SnapGroupController::OnOverviewModeStarting|Ending()`,
      // we need to refresh the stacking order when it's shown again.
      refresh_stacking_order = true;
    }
  } else if (update_visibility) {
    divider_widget_->Hide();
    // Else no need to refresh the stacking order if the divider is hidden.
    refresh_stacking_order = false;
  }

  if (refresh_stacking_order) {
    RefreshStackingOrder();
  }
}

void SplitViewDivider::CreateDividerWidget(int divider_position) {
  DCHECK(!divider_widget_);
  CHECK_GE(observed_windows_.size(), 1u);
  // Native widget owns this widget.
  divider_widget_ = new SplitViewDividerWidget;
  divider_widget_->set_focus_on_creation(false);
  aura::Window* parent_container = nullptr;
  aura::Window* top_window = window_util::GetTopMostWindow(observed_windows_);
  CHECK(top_window);
  parent_container = top_window->parent();
  CHECK(parent_container);

  const gfx::Rect initial_divider_bounds = GetDividerBoundsInScreen(
      GetWorkAreaBoundsInScreen(observed_windows_[0].get()),
      IsLayoutHorizontal(observed_windows_[0].get()), divider_position,
      /*is_dragging=*/false);
  divider_widget_->Init(
      CreateWidgetInitParams(parent_container, initial_divider_bounds));
  divider_widget_->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);
  divider_view_ = divider_widget_->SetContentsView(
      std::make_unique<SplitViewDividerView>(this));
  auto* divider_widget_native_window = divider_widget_->GetNativeWindow();
  // TODO(michelefan|sophiewen): Evaluate and remove this property if needed.
  divider_widget_native_window->SetProperty(kLockedToRootKey, true);

  // Use a window targeter and enlarge the hit region to allow located events
  // that are slightly outside the divider widget bounds be consumed by
  // `divider_widget_`.
  auto window_targeter = std::make_unique<aura::WindowTargeter>();
  window_targeter->SetInsets(gfx::Insets::VH(-kSplitViewDividerExtraInset,
                                             -kSplitViewDividerExtraInset));
  divider_widget_native_window->SetEventTargeter(std::move(window_targeter));

  // Explicitly `set_parent_controls_lifetime` to false so that the lifetime of
  // the divider will only be managed by `this`, which avoids UAF on window
  // destroying.
  wm::TransientWindowManager::GetOrCreate(divider_widget_native_window)
      ->set_parent_controls_lifetime(false);

  Shell::Get()->focus_cycler()->AddWidget(divider_widget_);
}

void SplitViewDivider::CloseDividerWidget() {
  // If we're shutting down, no need to refresh the stacking order. This isn't
  // needed but simply added for efficiency.
  base::AutoReset<bool> lock(&is_refreshing_stacking_order_, true);
  while (!observed_windows_.empty()) {
    MaybeRemoveObservedWindow(observed_windows_.back());
  }
  CHECK(!transient_windows_observations_.IsObservingAnySource());

  dragged_window_ = nullptr;

  if (divider_widget_) {
    Shell::Get()->focus_cycler()->RemoveWidget(divider_widget_);
    auto* divider_window = divider_widget_->GetNativeWindow();
    if (auto* transient_parent = wm::GetTransientParent(divider_window)) {
      wm::RemoveTransientChild(transient_parent, divider_window);
    }

    // During the asynchronous window closing, there may be a duration when the
    // divider widget is closing, but the pointer to `this` is not cleared in
    // `SplitViewDividerView` yet, i.e. in `Layout()` which is called during
    // clamshell <-> tablet transition.
    divider_view_->OnDividerClosing();
    // Disable any event handling on the divider while we are closing the
    // widget.
    divider_view_->SetCanProcessEventsWithinSubtree(false);
    divider_window->SetEventTargetingPolicy(aura::EventTargetingPolicy::kNone);
    divider_widget_->Close();
    divider_view_ = nullptr;
    divider_widget_ = nullptr;
  }
}

bool SplitViewDivider::GetActualTargetVisibility() const {
  return divider_widget_ &&
         divider_widget_->GetNativeWindow()->TargetVisibility();
}

void SplitViewDivider::RefreshStackingOrder() {
  // Skip the recursive update.
  if (is_refreshing_stacking_order_) {
    return;
  }

  base::AutoReset<bool> lock(&is_refreshing_stacking_order_, true);

  if (observed_windows_.empty() || !divider_widget_ ||
      !divider_widget_->IsVisible()) {
    return;
  }

  aura::Window::Windows visible_observed_windows;
  for (aura::Window* window : observed_windows_) {
    // During desk switches, the `IsVisible()`, which traverses up the
    // parent layer hierarchy to determine visibility, can be unreliable due to
    // the inactive desk's invisibility. Instead, use `TargetVisibility()`,
    // which directly assesses the window's target visibility, regardless of the
    // visibility of its parent's layer.
    if (window->TargetVisibility()) {
      visible_observed_windows.push_back(window);
    }
  }

  // To get `divider_window` prepared to be the transient window of the
  // `top_window` below, remove `divider_window` as the transient child from its
  // transient parent if any.
  auto* divider_window = divider_widget_->GetNativeWindow();
  if (auto* transient_parent = wm::GetTransientParent(divider_window)) {
    wm::RemoveTransientChild(transient_parent, divider_window);
  }

  CHECK(!wm::GetTransientParent(divider_window));

  if (visible_observed_windows.empty()) {
    divider_window->Hide();
    return;
  }

  aura::Window* top_window =
      window_util::GetTopMostWindow(visible_observed_windows);
  if (!top_window) {
    divider_window->Hide();
    return;
  }

  CHECK(top_window->TargetVisibility());

  auto* divider_sibling_window =
      dragged_window_ ? dragged_window_.get() : top_window;
  CHECK(divider_sibling_window);

  // The divider needs to have the same parent of the `divider_sibling_window`
  // otherwise we need to reparent the divider as below.
  if (divider_sibling_window->parent() != divider_window->parent()) {
    views::Widget::ReparentNativeView(divider_window,
                                      divider_sibling_window->parent());
  }

  if (dragged_window_) {
    divider_window->parent()->StackChildBelow(divider_window, dragged_window_);
    return;
  }

  // Refresh the stacking order of the other window.
  aura::Window* top_window_parent = top_window->parent();
  // Keep a copy as the order of children will be changed while iterating.
  const auto children = top_window_parent->children();

  // Iterate through the siblings of the top window in an increasing z-order
  // which reflects the relative order of siblings.
  for (aura::Window* window : children) {
    if (!base::Contains(visible_observed_windows, window) ||
        window == top_window) {
      continue;
    }

    top_window_parent->StackChildAbove(window, top_window);
    top_window_parent->StackChildAbove(top_window, window);
  }

  // Add the `divider_window` as a transient child of the `top_window`. In
  // this way, on new transient window added, the divider will be stacked above
  // the `top_window` but under the new transient window which is handled in
  // `TransientWindowManager::RestackTransientDescendants()`.
  wm::AddTransientChild(top_window, divider_window);

  top_window_parent->StackChildAbove(divider_window, top_window);
}

void SplitViewDivider::StartObservingTransientChild(aura::Window* transient) {
  // Confine the bounds of a transient window if the given `transient` is a
  // bubble dialog or dialog window.
  if (!window_util::AsBubbleDialogDelegate(transient) &&
      !window_util::AsDialogDelegate(transient)) {
    return;
  }

  // Explicitly check and early return if the `transient` is the divider native
  // window.
  if (divider_widget_ && transient == divider_widget_->GetNativeWindow()) {
    return;
  }

  DCHECK(!transient_windows_observations_.IsObservingSource(transient));

  // At this moment, the transient window may not have the valid bounds yet.
  // Start observing the transient window.
  transient_windows_observations_.AddObservation(transient);
}

void SplitViewDivider::StopObservingTransientChild(aura::Window* transient) {
  if (transient_windows_observations_.IsObservingSource(transient))
    transient_windows_observations_.RemoveObservation(transient);
}

gfx::Point SplitViewDivider::GetEndDragLocationInScreen(
    aura::Window* window) const {
  DCHECK(base::Contains(observed_windows_, window));
  gfx::Point end_location(previous_event_location_);

  const SnapPosition snap_position =
      controller_->GetPositionOfSnappedWindow(window);
  const gfx::Rect bounds = controller_->GetSnappedWindowBoundsInScreen(
      snap_position, window, window_util::GetSnapRatioForWindow(window),
      /*account_for_divider_width=*/true);

  const bool is_physical_left_or_top =
      IsPhysicallyLeftOrTop(snap_position, window);
  if (IsLayoutHorizontal(window)) {
    end_location.set_x(is_physical_left_or_top ? bounds.right() : bounds.x());
  } else {
    end_location.set_y(is_physical_left_or_top ? bounds.bottom() : bounds.y());
  }
  return end_location;
}

void SplitViewDivider::FinishWindowResizing() {
  for (aura::Window* window : observed_windows_) {
    WindowState* window_state = WindowState::Get(window);
    if (window_state->is_dragged()) {
      window_state->OnCompleteDrag(
          gfx::PointF(GetEndDragLocationInScreen(window)));
      window_state->DeleteDragDetails();
    }
  }
}

}  // namespace ash

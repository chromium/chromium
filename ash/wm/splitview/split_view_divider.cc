// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_divider.h"

#include <memory>

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider_handler_view.h"
#include "ash/wm/splitview/split_view_divider_view.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_properties.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

bool IsInTabletMode() {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  return tablet_mode_controller && tablet_mode_controller->InTabletMode();
}

}  // namespace

// -----------------------------------------------------------------------------
// AlwaysOnTopWindowTargeter:

// The window targeter that is installed on the always on top container window
// when the split view mode is active.
class AlwaysOnTopWindowTargeter : public aura::WindowTargeter {
 public:
  explicit AlwaysOnTopWindowTargeter(aura::Window* divider_window)
      : divider_window_(divider_window) {}

  AlwaysOnTopWindowTargeter(const AlwaysOnTopWindowTargeter&) = delete;
  AlwaysOnTopWindowTargeter& operator=(const AlwaysOnTopWindowTargeter&) =
      delete;
  ~AlwaysOnTopWindowTargeter() override = default;

 private:
  bool GetHitTestRects(aura::Window* target,
                       gfx::Rect* hit_test_rect_mouse,
                       gfx::Rect* hit_test_rect_touch) const override {
    if (target == divider_window_) {
      *hit_test_rect_mouse = *hit_test_rect_touch = gfx::Rect(target->bounds());
      hit_test_rect_touch->Inset(
          gfx::Insets::VH(-SplitViewDivider::kDividerEdgeInsetForTouch,
                          -SplitViewDivider::kDividerEdgeInsetForTouch));
      return true;
    }
    return aura::WindowTargeter::GetHitTestRects(target, hit_test_rect_mouse,
                                                 hit_test_rect_touch);
  }

  aura::Window* divider_window_;
};

// -----------------------------------------------------------------------------
// SplitViewDivider:

SplitViewDivider::SplitViewDivider(SplitViewController* controller)
    : controller_(controller) {
  Shell::Get()->activation_client()->AddObserver(this);
  CreateDividerWidget(controller);

  aura::Window* always_on_top_container = Shell::GetContainer(
      controller->root_window(), kShellWindowId_AlwaysOnTopContainer);
  split_view_window_targeter_ = std::make_unique<aura::ScopedWindowTargeter>(
      always_on_top_container, std::make_unique<AlwaysOnTopWindowTargeter>(
                                   divider_widget_->GetNativeWindow()));

  // Observe currently snapped windows.
  for (auto snap_pos : {SplitViewController::SnapPosition::kPrimary,
                        SplitViewController::SnapPosition::kSecondary}) {
    auto* window = controller_->GetSnappedWindow(snap_pos);
    if (window) {
      AddObservedWindow(window);
    }
  }
}

SplitViewDivider::~SplitViewDivider() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  divider_widget_->Close();
  split_view_window_targeter_.reset();
  for (auto* window : observed_windows_) {
    window->RemoveObserver(this);
    ::wm::TransientWindowManager::GetOrCreate(window)->RemoveObserver(this);
  }
  observed_windows_.clear();
}

// static
gfx::Rect SplitViewDivider::GetDividerBoundsInScreen(
    const gfx::Rect& work_area_bounds_in_screen,
    bool landscape,
    int divider_position,
    bool is_dragging) {
  const int dragging_diff = (kSplitviewDividerEnlargedShortSideLength -
                             kSplitviewDividerShortSideLength) /
                            2;
  if (landscape) {
    return is_dragging
               ? gfx::Rect(work_area_bounds_in_screen.x() + divider_position -
                               dragging_diff,
                           work_area_bounds_in_screen.y(),
                           kSplitviewDividerEnlargedShortSideLength,
                           work_area_bounds_in_screen.height())
               : gfx::Rect(work_area_bounds_in_screen.x() + divider_position,
                           work_area_bounds_in_screen.y(),
                           kSplitviewDividerShortSideLength,
                           work_area_bounds_in_screen.height());
  } else {
    return is_dragging
               ? gfx::Rect(work_area_bounds_in_screen.x(),
                           work_area_bounds_in_screen.y() + divider_position -
                               dragging_diff,
                           work_area_bounds_in_screen.width(),
                           kSplitviewDividerEnlargedShortSideLength)
               : gfx::Rect(work_area_bounds_in_screen.x(),
                           work_area_bounds_in_screen.y() + divider_position,
                           work_area_bounds_in_screen.width(),
                           kSplitviewDividerShortSideLength);
  }
}

void SplitViewDivider::DoSpawningAnimation(int spawning_position) {
  static_cast<SplitViewDividerView*>(divider_widget_->GetContentsView())
      ->DoSpawningAnimation(spawning_position);
}

void SplitViewDivider::UpdateDividerBounds() {
  divider_widget_->SetBounds(GetDividerBoundsInScreen(/*is_dragging=*/false));
}

gfx::Rect SplitViewDivider::GetDividerBoundsInScreen(bool is_dragging) {
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          controller_->root_window()->GetChildById(
              desks_util::GetActiveDeskContainerId()));
  const int divider_position = controller_->divider_position();
  const bool landscape = IsCurrentScreenOrientationLandscape();
  return GetDividerBoundsInScreen(work_area_bounds_in_screen, landscape,
                                  divider_position, is_dragging);
}

void SplitViewDivider::SetAlwaysOnTop(bool on_top) {
  if (on_top) {
    divider_widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingUIElement);

    // Special handling when put divider into always_on_top container. We want
    // to put it at the bottom so it won't block other always_on_top windows.
    aura::Window* always_on_top_container =
        Shell::GetContainer(divider_widget_->GetNativeWindow()->GetRootWindow(),
                            kShellWindowId_AlwaysOnTopContainer);
    always_on_top_container->StackChildAtBottom(
        divider_widget_->GetNativeWindow());
  } else {
    divider_widget_->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  }
}

void SplitViewDivider::StackOnTopOfTheObservedWindows() {
  divider_widget_->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  aura::Window* root_window = controller_->root_window();
  DCHECK(root_window);
  DCHECK(root_window->IsRootWindow());
  auto* parent_container =
      root_window->GetChildById(kShellWindowId_MenuContainer);
  auto* parent_container_layer = parent_container->layer();
  for (auto* window : observed_windows_) {
    if (window->parent() == parent_container) {
      parent_container->StackChildAtTop(window);
      parent_container_layer->StackAtTop(window->layer());
    }
  }

  aura::Window* divider_native_window = divider_widget_->GetNativeWindow();
  parent_container->StackChildAtTop(divider_native_window);
  parent_container_layer->StackAtTop(divider_native_window->layer());
}

void SplitViewDivider::SetAdjustable(bool adjustable) {
  if (adjustable == IsAdjustable()) {
    return;
  }

  divider_widget_->GetNativeWindow()->SetEventTargetingPolicy(
      adjustable ? aura::EventTargetingPolicy::kTargetAndDescendants
                 : aura::EventTargetingPolicy::kNone);
  static_cast<SplitViewDividerView*>(divider_view_)
      ->SetDividerBarVisible(adjustable);
}

bool SplitViewDivider::IsAdjustable() const {
  DCHECK(divider_widget_);
  DCHECK(divider_widget_->GetNativeView());
  return divider_widget_->GetNativeWindow()->event_targeting_policy() !=
         aura::EventTargetingPolicy::kNone;
}

void SplitViewDivider::AddObservedWindow(aura::Window* window) {
  if (!base::Contains(observed_windows_, window)) {
    window->AddObserver(this);
    observed_windows_.push_back(window);
    ::wm::TransientWindowManager* transient_manager =
        ::wm::TransientWindowManager::GetOrCreate(window);
    transient_manager->AddObserver(this);
    for (auto* transient_window : transient_manager->transient_children())
      StartObservingTransientChild(transient_window);
  }
}

void SplitViewDivider::RemoveObservedWindow(aura::Window* window) {
  auto iter = base::ranges::find(observed_windows_, window);
  if (iter != observed_windows_.end()) {
    window->RemoveObserver(this);
    observed_windows_.erase(iter);
    ::wm::TransientWindowManager* transient_manager =
        ::wm::TransientWindowManager::GetOrCreate(window);
    transient_manager->RemoveObserver(this);
    for (auto* transient_window : transient_manager->transient_children())
      StopObservingTransientChild(transient_window);
  }
}

void SplitViewDivider::OnWindowDragStarted() {
  is_dragging_window_ = true;
  SetAlwaysOnTop(false);

  aura::Window* divider_window = divider_widget_->GetNativeWindow();
  divider_window->parent()->StackChildAtBottom(divider_window);
}

void SplitViewDivider::OnWindowDragEnded() {
  is_dragging_window_ = false;
  if (IsInTabletMode()) {
    SetAlwaysOnTop(true);
  } else {
    StackOnTopOfTheObservedWindows();
  }
}

bool SplitViewDivider::IsWindowObserved(const aura::Window* window) const {
  return base::Contains(observed_windows_, window);
}

void SplitViewDivider::OnWindowDestroying(aura::Window* window) {
  RemoveObservedWindow(window);
}

void SplitViewDivider::OnWindowBoundsChanged(aura::Window* window,
                                             const gfx::Rect& old_bounds,
                                             const gfx::Rect& new_bounds,
                                             ui::PropertyChangeReason reason) {
  if (!controller_->InSplitViewMode())
    return;

  // We only care about the bounds change of windows in
  // |transient_windows_observations_|.
  if (!transient_windows_observations_.IsObservingSource(window))
    return;

  // |window|'s transient parent must be one of the windows in
  // |observed_windows_|.
  aura::Window* transient_parent = nullptr;
  for (auto* observed_window : observed_windows_) {
    if (::wm::HasTransientAncestor(window, observed_window)) {
      transient_parent = observed_window;
      break;
    }
  }
  DCHECK(transient_parent);

  gfx::Rect transient_bounds = window->GetBoundsInScreen();
  transient_bounds.AdjustToFit(transient_parent->GetBoundsInScreen());
  window->SetBoundsInScreen(
      transient_bounds,
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
}

void SplitViewDivider::OnWindowActivated(ActivationReason reason,
                                         aura::Window* gained_active,
                                         aura::Window* lost_active) {
  if (IsInTabletMode() && !is_dragging_window_ &&
      (!gained_active || base::Contains(observed_windows_, gained_active))) {
    SetAlwaysOnTop(true);
  } else if (ShouldAutomaticallyGroupOnWindowsSnappedInClamshell()) {
    StackOnTopOfTheObservedWindows();
  } else {
    // If `gained_active` is not one of the observed windows, or there is one
    // window that is currently being dragged, `divider_widget_` should not
    // be placed on top.
    SetAlwaysOnTop(false);
  }
}

void SplitViewDivider::OnTransientChildAdded(aura::Window* window,
                                             aura::Window* transient) {
  StartObservingTransientChild(transient);
}

void SplitViewDivider::OnTransientChildRemoved(aura::Window* window,
                                               aura::Window* transient) {
  StopObservingTransientChild(transient);
}

void SplitViewDivider::CreateDividerWidget(SplitViewController* controller) {
  DCHECK(!divider_widget_);
  // Native widget owns this widget.
  divider_widget_ = new views::Widget;
  divider_widget_->set_focus_on_creation(false);
  divider_widget_->Init(
      CreateWidgetInitParams(controller->root_window(), "SplitViewDivider"));
  divider_widget_->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);
  divider_view_ = divider_widget_->SetContentsView(
      std::make_unique<SplitViewDividerView>(controller, this));
  divider_widget_->SetBounds(GetDividerBoundsInScreen(/*is_dragging=*/false));
  auto* divider_widget_native_window = divider_widget_->GetNativeWindow();
  divider_widget_native_window->SetProperty(kLockedToRootKey, true);

  // Use a window targeter and enlarge the hit region to allow located events
  // that are slightly outside the divider widget bounds be consumed by
  // `divider_widget_`.
  auto window_targeter = std::make_unique<aura::WindowTargeter>();
  window_targeter->SetInsets(kSplitViewDividerExtraInset);
  divider_widget_native_window->SetEventTargeter(std::move(window_targeter));

  divider_widget_->Show();
}

void SplitViewDivider::StartObservingTransientChild(aura::Window* transient) {
  // For now, we only care about dialog bubbles type transient child. We may
  // observe other types transient child window as well if need arises in the
  // future.
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(transient);
  if (!widget || !widget->widget_delegate()->AsBubbleDialogDelegate())
    return;

  // At this moment, the transient window may not have the valid bounds yet.
  // Start observe the transient window.
  transient_windows_observations_.AddObservation(transient);
}

void SplitViewDivider::StopObservingTransientChild(aura::Window* transient) {
  if (transient_windows_observations_.IsObservingSource(transient))
    transient_windows_observations_.RemoveObservation(transient);
}

}  // namespace ash

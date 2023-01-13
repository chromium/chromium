// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_controller.h"

#include <cmath>
#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/delayed_animation_observer_impl.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_metrics_controller.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/cxx17_backports.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "base/system/sys_info.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/wm/features.h"
#include "components/app_restore/desk_template_read_handler.h"
#include "components/app_restore/window_properties.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/input_method.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/compositor_animation_runner.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

using ::chromeos::WindowStateType;

// Three fixed position ratios of the divider, which means the divider can
// always be moved to these three positions.
constexpr float kFixedPositionRatios[] = {0.f, 0.5f, 1.0f};

// The black scrim starts to fade in when the divider is moved past the two
// optional positions (kOneThirdSnapRatio, kTwoThirdSnapRatio) and
// reaches to its maximum opacity (kBlackScrimOpacity) after moving
// kBlackScrimFadeInRatio of the screen width. See https://crbug.com/827730 for
// details.
constexpr float kBlackScrimFadeInRatio = 0.1f;
constexpr float kBlackScrimOpacity = 0.4f;

// The speed at which the divider is moved controls whether windows are scaled
// or translated. If the divider is moved more than this many pixels per second,
// the "fast" mode is enabled.
constexpr int kSplitViewThresholdPixelsPerSec = 72;

// This is how often the divider drag speed is checked.
constexpr base::TimeDelta kSplitViewChunkTime = base::Milliseconds(500);

// Records the animation smoothness when the divider is released during a resize
// and animated to a fixed position ratio.
constexpr char kDividerAnimationSmoothness[] =
    "Ash.SplitViewResize.AnimationSmoothness.DividerAnimation";

// Histogram names that record presentation time of resize operation with
// following conditions, a) clamshell split view, empty overview grid,
// b) clamshell split view, nonempty overview grid, c) tablet split view, one
// snapped window, empty overview grid, d) tablet split view, two snapped
// windows, e) tablet split view, one snapped window, nonempty overview grid.
constexpr char kClamshellSplitViewResizeSingleHistogram[] =
    "Ash.SplitViewResize.PresentationTime.ClamshellMode.SingleWindow";
constexpr char kClamshellSplitViewResizeWithOverviewHistogram[] =
    "Ash.SplitViewResize.PresentationTime.ClamshellMode.WithOverview";
constexpr char kTabletSplitViewResizeSingleHistogram[] =
    "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow";
constexpr char kTabletSplitViewResizeMultiHistogram[] =
    "Ash.SplitViewResize.PresentationTime.TabletMode.MultiWindow";
constexpr char kTabletSplitViewResizeWithOverviewHistogram[] =
    "Ash.SplitViewResize.PresentationTime.TabletMode.WithOverview";

constexpr char kClamshellSplitViewResizeSingleMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.ClamshellMode."
    "SingleWindow";
constexpr char kClamshellSplitViewResizeWithOverviewMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.ClamshellMode."
    "WithOverview";
constexpr char kTabletSplitViewResizeSingleMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow";
constexpr char kTabletSplitViewResizeMultiMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.MultiWindow";
constexpr char kTabletSplitViewResizeWithOverviewMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.WithOverview";

// The time when the number of roots in split view changes from one to two. Used
// for the purpose of metric collection.
base::Time g_multi_display_split_view_start_time;

bool IsExactlyOneRootInSplitView() {
  const aura::Window::Windows all_root_windows = Shell::GetAllRootWindows();
  return 1 ==
         base::ranges::count_if(
             all_root_windows, [](aura::Window* root_window) {
               return SplitViewController::Get(root_window)->InSplitViewMode();
             });
}

gfx::Point GetBoundedPosition(const gfx::Point& location_in_screen,
                              const gfx::Rect& bounds_in_screen) {
  return gfx::Point(base::clamp(location_in_screen.x(), bounds_in_screen.x(),
                                bounds_in_screen.right() - 1),
                    base::clamp(location_in_screen.y(), bounds_in_screen.y(),
                                bounds_in_screen.bottom() - 1));
}

ui::InputMethod* GetCurrentInputMethod() {
  if (auto* bridge = IMEBridge::Get()) {
    if (auto* handler = bridge->GetInputContextHandler())
      return handler->GetInputMethod();
  }
  return nullptr;
}

WindowStateType GetStateTypeFromSnapPosition(
    SplitViewController::SnapPosition snap_position) {
  DCHECK(snap_position != SplitViewController::SnapPosition::kNone);
  if (snap_position == SplitViewController::SnapPosition::kPrimary)
    return WindowStateType::kPrimarySnapped;
  if (snap_position == SplitViewController::SnapPosition::kSecondary)
    return WindowStateType::kSecondarySnapped;
  NOTREACHED();
  return WindowStateType::kDefault;
}

// Returns the minimum length of the window according to the screen orientation.
int GetMinimumWindowLength(aura::Window* window, bool horizontal) {
  int minimum_width = 0;
  if (window && window->delegate()) {
    gfx::Size minimum_size = window->delegate()->GetMinimumSize();
    minimum_width = horizontal ? minimum_size.width() : minimum_size.height();
  }
  return minimum_width;
}

// Returns the length of the window according to the screen orientation.
int GetWindowLength(aura::Window* window, bool horizontal) {
  const auto& bounds = window->bounds();
  return horizontal ? bounds.width() : bounds.height();
}

// Returns true if |window| is currently snapped.
bool IsSnapped(aura::Window* window) {
  if (!window)
    return false;
  return WindowState::Get(window)->IsSnapped();
}

// Returns the overview session if overview mode is active, otherwise returns
// nullptr.
OverviewSession* GetOverviewSession() {
  return Shell::Get()->overview_controller()->InOverviewSession()
             ? Shell::Get()->overview_controller()->overview_session()
             : nullptr;
}

void RemoveSnappingWindowFromOverviewIfApplicable(
    OverviewSession* overview_session,
    aura::Window* window) {
  if (!overview_session)
    return;
  OverviewItem* item = overview_session->GetOverviewItemForWindow(window);
  if (!item)
    return;
  // Remove it from overview. The transform will be reset later after the window
  // is snapped. Note the remaining windows in overview don't need to be
  // repositioned in this case as they have been positioned to the right place
  // during dragging.
  item->RestoreWindow(/*reset_transform=*/false);
  overview_session->RemoveItem(item);
}

void UpdateSnappedBounds(aura::Window* window) {
  WindowState* window_state = WindowState::Get(window);
  window_state->UpdateSnapRatio();
  const bool in_tablet = Shell::Get()->tablet_mode_controller()->InTabletMode();
  if (in_tablet) {
    // TODO(b/264962634): Remove this workaround. Probably, we can rewrite
    // `TabletModeWindowState::UpdateWindowPosition` to include this logic.
    if (window->GetProperty(aura::client::kAppType) ==
        static_cast<int>(AppType::ARC_APP)) {
      const SetBoundsWMEvent event(
          TabletModeWindowState::GetBoundsInTabletMode(window_state),
          /*animate=*/true);
      window_state->OnWMEvent(&event);
      return;
    }
    TabletModeWindowState::UpdateWindowPosition(
        window_state, WindowState::BoundsChangeAnimationType::kAnimate);
  } else {
    window_state->UpdateSnappedBounds();
  }
}

}  // namespace

// The window observer that observes the current tab-dragged window. When it's
// created, it observes the dragged window, and there are two possible results
// after the user finishes dragging: 1) the dragged window stays a new window
// and SplitViewController needs to decide where to put the window; 2) the
// dragged window's tabs are attached into another browser window and thus is
// destroyed.
class SplitViewController::TabDraggedWindowObserver
    : public aura::WindowObserver {
 public:
  TabDraggedWindowObserver(
      SplitViewController* split_view_controller,
      aura::Window* dragged_window,
      SplitViewController::SnapPosition desired_snap_position,
      const gfx::Point& last_location_in_screen)
      : split_view_controller_(split_view_controller),
        dragged_window_(dragged_window),
        desired_snap_position_(desired_snap_position),
        last_location_in_screen_(last_location_in_screen) {
    DCHECK(window_util::IsDraggingTabs(dragged_window_));
    dragged_window_->AddObserver(this);
  }

  TabDraggedWindowObserver(const TabDraggedWindowObserver&) = delete;
  TabDraggedWindowObserver& operator=(const TabDraggedWindowObserver&) = delete;

  ~TabDraggedWindowObserver() override {
    if (dragged_window_)
      dragged_window_->RemoveObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    // At this point we know the newly created dragged window is going to be
    // destroyed due to all of its tabs are attaching into another window.
    EndTabDragging(window, /*is_being_destroyed=*/true);
  }

  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK_EQ(window, dragged_window_);
    if (key == kIsDraggingTabsKey && !window_util::IsDraggingTabs(window)) {
      // At this point we know the newly created dragged window just finished
      // dragging.
      EndTabDragging(window, /*is_being_destroyed=*/false);
    }
  }

 private:
  // Called after the tab dragging is ended, the dragged window is either
  // destroyed because of merging into another window, or stays as a separate
  // window.
  void EndTabDragging(aura::Window* window, bool is_being_destroyed) {
    dragged_window_->RemoveObserver(this);
    dragged_window_ = nullptr;

    WindowState::Get(window)->set_snap_action_source(
        WindowSnapActionSource::kDragTabToSnap);
    split_view_controller_->EndWindowDragImpl(window, is_being_destroyed,
                                              desired_snap_position_,
                                              last_location_in_screen_);

    // Update the source window's bounds if applicable.
    UpdateSourceWindowBoundsAfterDragEnds(window);
  }

  // The source window might have been scaled down during dragging, we should
  // update its bounds to ensure it has the right bounds after the drag ends.
  void UpdateSourceWindowBoundsAfterDragEnds(aura::Window* window) {
    aura::Window* source_window =
        window->GetProperty(kTabDraggingSourceWindowKey);
    if (source_window) {
      TabletModeWindowState::UpdateWindowPosition(
          WindowState::Get(source_window),
          WindowState::BoundsChangeAnimationType::kAnimate);
    }
  }

  SplitViewController* split_view_controller_;
  aura::Window* dragged_window_;
  SplitViewController::SnapPosition desired_snap_position_;
  gfx::Point last_location_in_screen_;
};

// Animates the divider to its closest fixed position.
// SplitViewController::is_resizing_ is assumed to be already set to false
// before this animation starts, but some resizing logic is delayed until this
// animation ends.
class SplitViewController::DividerSnapAnimation
    : public gfx::SlideAnimation,
      public gfx::AnimationDelegate {
 public:
  DividerSnapAnimation(SplitViewController* split_view_controller,
                       int starting_position,
                       int ending_position)
      : gfx::SlideAnimation(this),
        split_view_controller_(split_view_controller),
        starting_position_(starting_position),
        ending_position_(ending_position) {
    // Before you change this value, read the comment on kIsWindowMovedTimeoutMs
    // in tablet_mode_window_drag_delegate.cc.
    SetSlideDuration(base::Milliseconds(300));
    SetTweenType(gfx::Tween::EASE_IN);

    aura::Window* window = split_view_controller->primary_window()
                               ? split_view_controller->primary_window()
                               : split_view_controller->secondary_window();
    DCHECK(window);

    // |widget| may be null in tests. It will use the default animation
    // container in this case.
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    if (!widget)
      return;

    gfx::AnimationContainer* container = new gfx::AnimationContainer();
    container->SetAnimationRunner(
        std::make_unique<views::CompositorAnimationRunner>(widget, FROM_HERE));
    SetContainer(container);

    tracker_.emplace(widget->GetCompositor()->RequestNewThroughputTracker());
    tracker_->Start(
        metrics_util::ForSmoothness(base::BindRepeating([](int smoothness) {
          UMA_HISTOGRAM_PERCENTAGE(kDividerAnimationSmoothness, smoothness);
        })));
  }
  DividerSnapAnimation(const DividerSnapAnimation&) = delete;
  DividerSnapAnimation& operator=(const DividerSnapAnimation&) = delete;
  ~DividerSnapAnimation() override = default;

  int ending_position() const { return ending_position_; }

 private:
  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override {
    DCHECK(split_view_controller_->InSplitViewMode());
    DCHECK(!split_view_controller_->is_resizing_);
    DCHECK_EQ(ending_position_, split_view_controller_->divider_position_);

    split_view_controller_->EndResizeImpl();
    split_view_controller_->EndTabletSplitViewAfterResizingIfAppropriate();

    if (tracker_)
      tracker_->Stop();
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    DCHECK(split_view_controller_->InSplitViewMode());
    DCHECK(!split_view_controller_->is_resizing_);

    split_view_controller_->divider_position_ =
        CurrentValueBetween(starting_position_, ending_position_);
    split_view_controller_->NotifyDividerPositionChanged();
    split_view_controller_->UpdateSnappedWindowsAndDividerBounds();
    // Updating the window may stop animation.
    if (is_animating()) {
      split_view_controller_->UpdateResizeBackdrop();
      split_view_controller_->SetWindowsTransformDuringResizing();
    }
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    if (tracker_)
      tracker_->Cancel();
  }

  SplitViewController* split_view_controller_;
  int starting_position_;
  int ending_position_;
  absl::optional<ui::ThroughputTracker> tracker_;
};

// The controller that observes the window state and performs auto snapping
// for the window if needed. When it's created, it observes the root window
// and all windows in a current active desk. When 1) an observed window is
// activated or 2) changed to visible from minimized, this class performs
// auto snapping for the window if it's possible.
class SplitViewController::AutoSnapController
    : public wm::ActivationChangeObserver,
      public aura::WindowObserver {
 public:
  explicit AutoSnapController(SplitViewController* split_view_controller)
      : split_view_controller_(split_view_controller) {
    Shell::Get()->activation_client()->AddObserver(this);
    AddWindow(split_view_controller->root_window());
    for (auto* window :
         Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
      AddWindow(window);
    }
  }

  ~AutoSnapController() override {
    for (auto* window : observed_windows_)
      window->RemoveObserver(this);
    Shell::Get()->activation_client()->RemoveObserver(this);
  }

  AutoSnapController(const AutoSnapController&) = delete;
  AutoSnapController& operator=(const AutoSnapController&) = delete;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    if (!gained_active)
      return;

    // If |gained_active| was activated as a side effect of a window disposition
    // change, do nothing. For example, when a snapped window is closed, another
    // window will be activated before OnWindowDestroying() is called. We should
    // not try to snap another window in this case.
    if (reason == ActivationReason::WINDOW_DISPOSITION_CHANGED)
      return;

    AutoSnapWindowIfNeeded(gained_active);
  }

  // aura::WindowObserver:
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override {
    // When a minimized window's visibility changes from invisible to visible or
    // is about to activate, it triggers an implicit un-minimizing (e.g.
    // |WorkspaceLayoutManager::OnChildWindowVisibilityChanged| or
    // |WorkspaceLayoutManager::OnWindowActivating|). This emits a window
    // state change event but it is unnecessary for to-be-snapped windows
    // because some clients (e.g. ARC app) handle a window state change
    // asynchronously. So in the case, we here try to snap a window before
    // other's handling to avoid the implicit un-minimizing.

    // Auto snapping is applicable for window changed to be visible.
    if (!visible)
      return;

    // Already un-minimized windows are not applicable for auto snapping.
    if (!WindowState::Get(window) || !WindowState::Get(window)->IsMinimized())
      return;

    // Visibility changes while restoring windows after dragged is transient
    // hide & show operations so not applicable for auto snapping.
    if (window->GetProperty(kHideDuringWindowDragging))
      return;

    AutoSnapWindowIfNeeded(window);
  }

  void OnWindowAddedToRootWindow(aura::Window* window) override {
    AddWindow(window);
  }

  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override {
    RemoveWindow(window);
  }

  void OnWindowDestroying(aura::Window* window) override {
    RemoveWindow(window);
  }

 private:
  void AutoSnapWindowIfNeeded(aura::Window* window) {
    DCHECK(window);

    if (window->GetRootWindow() != split_view_controller_->root_window())
      return;

    // We perform an "auto" snapping only if split view mode is active.
    if (!split_view_controller_->InSplitViewMode())
      return;

    if (DesksController::Get()->AreDesksBeingModified()) {
      // Activating a desk from its mini view will activate its most-recently
      // used window, but this should not result in snapping and ending overview
      // mode now. Overview will be ended explicitly as part of the desk
      // activation animation.
      return;
    }

    // Only windows that are in the MRU list and are not already in split view
    // can be auto-snapped.
    if (split_view_controller_->IsWindowInSplitView(window) ||
        !base::Contains(
            Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk),
            window)) {
      return;
    }

    // We do not auto snap windows in clamshell splitview mode if a new window
    // is activated when clamshell splitview mode is active. In this case we'll
    // just end overview mode which will then end splitview mode.
    // TODO(xdai): Handle this logic in OverivewSession::OnWindowActivating().
    if (split_view_controller_->InClamshellSplitViewMode()) {
      Shell::Get()->overview_controller()->EndOverview(
          OverviewEndAction::kSplitView);
      return;
    }

    DCHECK(split_view_controller_->InTabletSplitViewMode());

    // Do not snap the window if the activation change is caused by dragging a
    // window, or by dragging a tab. Note the two values
    // WindowState::is_dragged() and IsDraggingTabs() might not be exactly the
    // same under certain circumstance, e.g., when a tab is dragged out from a
    // browser window, a new browser window will be created for the dragged tab
    // and then be activated, and at that time, IsDraggingTabs() is true, but
    // WindowState::is_dragged() is still false. And later after the window drag
    // starts, WindowState::is_dragged() will then be true.
    if (WindowState::Get(window)->is_dragged() ||
        window_util::IsDraggingTabs(window)) {
      return;
    }

    // If the divider is animating, then |window| cannot be snapped (and is
    // not already snapped either, because then we would have bailed out by
    // now). Then if |window| is user-positionable, we should end split view
    // mode, but the cannot snap toast would be inappropriate because the user
    // still might be able to snap |window|.
    if (split_view_controller_->IsDividerAnimating()) {
      if (WindowState::Get(window)->IsUserPositionable())
        split_view_controller_->EndSplitView(
            EndReason::kUnsnappableWindowActivated);
      return;
    }

    // If it's a user positionable window but can't be snapped, end split view
    // mode and show the cannot snap toast.
    if (!split_view_controller_->CanSnapWindow(window)) {
      if (WindowState::Get(window)->IsUserPositionable()) {
        split_view_controller_->EndSplitView(
            EndReason::kUnsnappableWindowActivated);
        ShowAppCannotSnapToast();
      }
      return;
    }

    // Snap the window on the non-default side of the screen if split view mode
    // is active.
    WindowState::Get(window)->set_snap_action_source(
        WindowSnapActionSource::kAutoSnapBySplitview);
    split_view_controller_->SnapWindow(
        window, (split_view_controller_->default_snap_position() ==
                 SnapPosition::kPrimary)
                    ? SnapPosition::kSecondary
                    : SnapPosition::kPrimary);
  }

  void AddWindow(aura::Window* window) {
    if (split_view_controller_->root_window() != window->GetRootWindow())
      return;

    if (!window->HasObserver(this))
      window->AddObserver(this);
    observed_windows_.insert(window);
  }

  void RemoveWindow(aura::Window* window) {
    window->RemoveObserver(this);
    observed_windows_.erase(window);
  }

  SplitViewController* split_view_controller_;

  // Tracks observed windows.
  base::flat_set<aura::Window*> observed_windows_;
};

// Helper class that prepares windows that are changing to snapped window state.
// This allows async window state type changes and handles calls to
// SplitViewController when necessary.
class SplitViewController::ToBeSnappedWindowsObserver
    : public aura::WindowObserver,
      public WindowStateObserver {
 public:
  explicit ToBeSnappedWindowsObserver(
      SplitViewController* split_view_controller)
      : split_view_controller_(split_view_controller) {}
  ToBeSnappedWindowsObserver(const ToBeSnappedWindowsObserver&) = delete;
  ToBeSnappedWindowsObserver& operator=(const ToBeSnappedWindowsObserver&) =
      delete;

  ~ToBeSnappedWindowsObserver() override {
    for (auto iter = to_be_snapped_windows_.begin();
         iter != to_be_snapped_windows_.end(); iter++) {
      aura::Window* window = iter->second;
      if (window) {
        window->RemoveObserver(this);
        WindowState::Get(window)->RemoveObserver(this);
      }
    }
    to_be_snapped_windows_.clear();
  }

  void AddToBeSnappedWindow(aura::Window* window,
                            SplitViewController::SnapPosition snap_position) {
    // If |window| is already snapped in split screen, do nothing.
    if (split_view_controller_->IsWindowInSplitView(window)) {
      if (WindowState::Get(window)->GetStateType() !=
          GetStateTypeFromSnapPosition(snap_position)) {
        // This can happen when swapping the positions of the two snapped
        // windows in the split view.
        split_view_controller_->AttachSnappingWindow(window, snap_position);
      }
      return;
    }

    aura::Window* old_window = to_be_snapped_windows_[snap_position];
    if (old_window == window)
      return;

    // Stop observe any previous to-be-snapped window in |snap_position|. This
    // can happen to Android windows as its window state and bounds change are
    // async, so it's possible to snap another window to the same position while
    // waiting for the snapping of the previous window.
    if (old_window) {
      to_be_snapped_windows_[snap_position] = nullptr;
      WindowState::Get(old_window)->RemoveObserver(this);
      old_window->RemoveObserver(this);
    }

    // If the to-be-snapped window already has the desired snapped window state,
    // no need to listen to the state change notification (there will be none
    // anyway), instead just attach the window to split screen directly.
    if (WindowState::Get(window)->GetStateType() ==
        GetStateTypeFromSnapPosition(snap_position)) {
      split_view_controller_->AttachSnappingWindow(window, snap_position);
      split_view_controller_->OnWindowSnapped(window,
                                              /*previous_state=*/absl::nullopt);
    } else {
      to_be_snapped_windows_[snap_position] = window;
      WindowState::Get(window)->AddObserver(this);
      window->AddObserver(this);
    }
  }

  bool IsObserving(const aura::Window* window) const {
    return FindWindow(window) != to_be_snapped_windows_.end();
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    auto iter = FindWindow(window);
    DCHECK(iter != to_be_snapped_windows_.end());
    window->RemoveObserver(this);
    WindowState::Get(window)->RemoveObserver(this);
    to_be_snapped_windows_.erase(iter);
  }

  // WindowStateObserver:
  void OnPreWindowStateTypeChange(WindowState* window_state,
                                  WindowStateType old_type) override {
    // When arriving here, we know the to-be-snapped window's state has just
    // changed and its bounds will be changed soon. Remove the window from
    // |to_be_snapped_windows_| and doing some prep work for snapping it in
    // split screen if applicable.
    auto iter = FindWindow(window_state->window());
    DCHECK(iter != to_be_snapped_windows_.end());
    SnapPosition snap_position = iter->first;
    to_be_snapped_windows_.erase(iter);
    window_state->RemoveObserver(this);
    window_state->window()->RemoveObserver(this);

    if (window_state->GetStateType() ==
        GetStateTypeFromSnapPosition(snap_position)) {
      split_view_controller_->AttachSnappingWindow(window_state->window(),
                                                   snap_position);
    }
  }

 private:
  std::map<SnapPosition, aura::Window*>::const_iterator FindWindow(
      const aura::Window* window) const {
    for (auto iter = to_be_snapped_windows_.begin();
         iter != to_be_snapped_windows_.end(); iter++) {
      if (iter->second == window)
        return iter;
    }
    return to_be_snapped_windows_.end();
  }

  SplitViewController* const split_view_controller_;
  // Tracks to-be-snapped windows.
  std::map<SnapPosition, aura::Window*> to_be_snapped_windows_;
};

// static
SplitViewController* SplitViewController::Get(const aura::Window* window) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());
  DCHECK(RootWindowController::ForWindow(window));
  return RootWindowController::ForWindow(window)->split_view_controller();
}

// static
bool SplitViewController::IsLayoutHorizontal(aura::Window* window) {
  return IsLayoutHorizontal(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
}

// static
bool SplitViewController::IsLayoutHorizontal(const display::Display& display) {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  if (tablet_mode_controller && tablet_mode_controller->InTabletMode())
    return IsCurrentScreenOrientationLandscape();
  // TODO(crbug.com/1233192): add DCHECK to avoid square size display.
  DCHECK(display.is_valid());
  return chromeos::IsLandscapeOrientation(GetSnapDisplayOrientation(display));
}

// static
bool SplitViewController::IsLayoutPrimary(aura::Window* window) {
  return IsLayoutPrimary(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
}

// static
bool SplitViewController::IsLayoutPrimary(const display::Display& display) {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  if (tablet_mode_controller && tablet_mode_controller->InTabletMode())
    return IsCurrentScreenOrientationPrimary();
  DCHECK(display.is_valid());
  return chromeos::IsPrimaryOrientation(GetSnapDisplayOrientation(display));
}

// static
bool SplitViewController::IsPhysicalLeftOrTop(SnapPosition position,
                                              aura::Window* window) {
  DCHECK_NE(SnapPosition::kNone, position);
  return position == (IsLayoutPrimary(window) ? SnapPosition::kPrimary
                                              : SnapPosition::kSecondary);
}

// static
bool SplitViewController::IsPhysicalLeftOrTop(SnapPosition position,
                                              const display::Display& display) {
  DCHECK_NE(SnapPosition::kNone, position);
  return position == (IsLayoutPrimary(display) ? SnapPosition::kPrimary
                                               : SnapPosition::kSecondary);
}

SplitViewController::SplitViewController(aura::Window* root_window)
    : root_window_(root_window),
      to_be_snapped_windows_observer_(
          std::make_unique<ToBeSnappedWindowsObserver>(this)),
      split_view_metrics_controller_(
          std::make_unique<SplitViewMetricsController>(this)) {
  Shell::Get()->accessibility_controller()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  split_view_type_ = Shell::Get()->tablet_mode_controller()->InTabletMode()
                         ? SplitViewType::kTabletType
                         : SplitViewType::kClamshellType;
}

SplitViewController::~SplitViewController() {
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  if (Shell::Get()->accessibility_controller())
    Shell::Get()->accessibility_controller()->RemoveObserver(this);
  EndSplitView(EndReason::kRootWindowDestroyed);
}

bool SplitViewController::InSplitViewMode() const {
  return state_ != State::kNoSnap;
}

bool SplitViewController::BothSnapped() const {
  return state_ == State::kBothSnapped;
}

bool SplitViewController::InClamshellSplitViewMode() const {
  return InSplitViewMode() && split_view_type_ == SplitViewType::kClamshellType;
}

bool SplitViewController::InTabletSplitViewMode() const {
  return InSplitViewMode() && split_view_type_ == SplitViewType::kTabletType;
}

bool SplitViewController::CanSnapWindow(aura::Window* window) const {
  if (!ShouldAllowSplitView())
    return false;

  if (!WindowState::Get(window)->CanSnapOnDisplay(
          display::Screen::GetScreen()->GetDisplayNearestWindow(
              const_cast<aura::Window*>(root_window_)))) {
    return false;
  }

  // Windows created by window restore are not activatable while being restored.
  // However, we still want to be able to snap these windows at this point.
  const bool is_to_be_restored_window =
      window == WindowRestoreController::Get()->to_be_snapped_window();

  // TODO(sammiequon): Investigate if we need to check for window activation.
  if (!is_to_be_restored_window && !wm::CanActivateWindow(window))
    return false;

  return GetMinimumWindowLength(window, IsLayoutHorizontal(window)) <=
         GetDividerEndPosition() / 2 - kSplitviewDividerShortSideLength / 2;
}

void SplitViewController::SnapWindow(aura::Window* window,
                                     SnapPosition snap_position,
                                     bool activate_window,
                                     float snap_ratio) {
  DCHECK(window && CanSnapWindow(window));
  DCHECK_NE(snap_position, SnapPosition::kNone);
  DCHECK(!is_resizing_);
  DCHECK(!IsDividerAnimating());

  OverviewSession* overview_session = GetOverviewSession();
  if (activate_window ||
      (overview_session &&
       overview_session->IsWindowActiveWindowBeforeOverview(window))) {
    to_be_activated_window_ = window;
  }

  to_be_snapped_windows_observer_->AddToBeSnappedWindow(window, snap_position);
  // Move |window| to the display of |root_window_| first before sending the
  // WMEvent. Otherwise it may be snapped to the wrong display.
  if (root_window_ != window->GetRootWindow()) {
    window_util::MoveWindowToDisplay(window,
                                     display::Screen::GetScreen()
                                         ->GetDisplayNearestWindow(root_window_)
                                         .id());
  }
  const WMEvent event(snap_position == SnapPosition::kPrimary
                          ? WM_EVENT_SNAP_PRIMARY
                          : WM_EVENT_SNAP_SECONDARY,
                      snap_ratio);
  WindowState::Get(window)->OnWMEvent(&event);

  base::RecordAction(base::UserMetricsAction("SplitView_SnapWindow"));
}

void SplitViewController::OnWMEvent(aura::Window* window,
                                    WMEventType event_type) {
  DCHECK(event_type == WM_EVENT_SNAP_PRIMARY ||
         event_type == WM_EVENT_SNAP_SECONDARY);

  // If split view can't be enabled at the moment, do nothing.
  if (!ShouldAllowSplitView())
    return;

  const bool in_overview =
      Shell::Get()->overview_controller()->InOverviewSession();

  // In clamshell mode, only if overview is active when receiving the WM event,
  // the window should be snapped in split screen. Otherwise, the window should
  // be snapped normally and should not be managed by SplitViewController.
  if (split_view_type_ == SplitViewType::kClamshellType && !in_overview)
    return;

  // If the snap wm event is from desk template launch when in overview, do not
  // try to snap the window in split screen. Otherwise, overview might be exited
  // because of window snapping.
  const int32_t window_id =
      window->GetProperty(app_restore::kRestoreWindowIdKey);
  if (in_overview &&
      window == WindowRestoreController::Get()->to_be_snapped_window() &&
      app_restore::DeskTemplateReadHandler::Get()->GetWindowInfo(window_id)) {
    return;
  }

  // Do nothing if |window| is already waiting to be snapped in split screen.
  if (to_be_snapped_windows_observer_->IsObserving(window))
    return;

  // If the snap event requested a snap ratio, update the divider position
  // so it can be used in `GetSnappedWindowBoundsInScreen()`.
  absl::optional<float> new_snap_ratio = WindowState::Get(window)->snap_ratio();
  if (new_snap_ratio) {
    divider_position_ = GetDividerPosition(event_type == WM_EVENT_SNAP_PRIMARY
                                               ? SnapPosition::kPrimary
                                               : SnapPosition::kSecondary,
                                           *new_snap_ratio);
    if (split_view_divider_) {
      split_view_divider_->UpdateDividerBounds();
    }
    if (state_ == State::kPrimarySnapped ||
        state_ == State::kSecondarySnapped) {
      // Only notify observers if there is only one snapped window, i.e.
      // overview is open.
      NotifyDividerPositionChanged();
    } else if (BothSnapped()) {
      // If both windows are snapped and one window snap ratio changes, the
      // other window should also get updated.
      UpdateSnappedWindowsAndDividerBounds();
    }
  }

  // Start observe the to-be-snapped window.
  to_be_snapped_windows_observer_->AddToBeSnappedWindow(
      window, event_type == WM_EVENT_SNAP_PRIMARY ? SnapPosition::kPrimary
                                                  : SnapPosition::kSecondary);
}

void SplitViewController::AttachSnappingWindow(aura::Window* window,
                                               SnapPosition snap_position) {
  // Save the transformed bounds in preparation for the snapping animation.
  UpdateSnappingWindowTransformedBounds(window);

  OverviewSession* overview_session = GetOverviewSession();
  RemoveSnappingWindowFromOverviewIfApplicable(overview_session, window);

  if (state_ == State::kNoSnap) {
    // Add observers when the split view mode starts.
    Shell::Get()->AddShellObserver(this);
    Shell::Get()->overview_controller()->AddObserver(this);
    if (features::IsAdjustSplitViewForVKEnabled()) {
      keyboard::KeyboardUIController::Get()->AddObserver(this);
      Shell::Get()->activation_client()->AddObserver(this);
    }

    auto_snap_controller_ = std::make_unique<AutoSnapController>(this);

    // Get the divider position given by `snap_ratio`, or if there is pre-set
    // |divider_position_|, use it. It can happen during tablet <-> clamshell
    // transition or multi-user transition.
    absl::optional<float> snap_ratio = WindowState::Get(window)->snap_ratio();
    divider_position_ =
        (divider_position_ < 0)
            ? GetDividerPosition(snap_position,
                                 snap_ratio ? *snap_ratio : kDefaultSnapRatio)
            : divider_position_;
    default_snap_position_ = snap_position;

    // There is no divider bar in clamshell splitview mode.
    if (split_view_type_ == SplitViewType::kTabletType) {
      split_view_divider_ = std::make_unique<SplitViewDivider>(this);
    }

    splitview_start_time_ = base::Time::Now();
    // We are about to enter split view on |root_window_|. If split view is
    // already active on exactly one root, then |root_window_| will be the
    // second root, and so multi-display split view begins now.
    if (IsExactlyOneRootInSplitView()) {
      base::RecordAction(
          base::UserMetricsAction("SplitView_MultiDisplaySplitView"));
      g_multi_display_split_view_start_time = splitview_start_time_;
    }
  }

  aura::Window* previous_snapped_window = nullptr;
  if (snap_position == SnapPosition::kPrimary) {
    if (primary_window_ != window) {
      previous_snapped_window = primary_window_;
      StopObserving(SnapPosition::kPrimary);
      primary_window_ = window;
    }
    if (secondary_window_ == window) {
      secondary_window_ = nullptr;
      default_snap_position_ = SnapPosition::kPrimary;
    }
  } else if (snap_position == SnapPosition::kSecondary) {
    if (secondary_window_ != window) {
      previous_snapped_window = secondary_window_;
      StopObserving(SnapPosition::kSecondary);
      secondary_window_ = window;
    }
    if (primary_window_ == window) {
      primary_window_ = nullptr;
      default_snap_position_ = SnapPosition::kSecondary;
    }
  }
  StartObserving(window);

  // Insert the previous snapped window to overview if overview is active.
  DCHECK_EQ(overview_session, GetOverviewSession());
  if (previous_snapped_window && overview_session) {
    InsertWindowToOverview(previous_snapped_window);
    // Ensure that the close icon will fade in. This part is redundant for
    // dragging from overview, but necessary for dragging from the top. For
    // dragging from overview, |OverviewItem::OnSelectorItemDragEnded| will be
    // called on all overview items including the |previous_snapped_window|
    // item anyway, whereas for dragging from the top,
    // |OverviewItem::OnSelectorItemDragEnded| already was called on all
    // overview items and |previous_snapped_window| was not yet among them.
    overview_session->GetOverviewItemForWindow(previous_snapped_window)
        ->OnSelectorItemDragEnded(/*snap=*/true);
  }

  if (split_view_type_ == SplitViewType::kTabletType) {
    divider_position_ = GetClosestFixedDividerPosition();
    split_view_divider_->UpdateDividerBounds();
  }

  base::RecordAction(base::UserMetricsAction("SplitView_SnapWindow"));
}

void SplitViewController::SwapWindows() {
  DCHECK(InSplitViewMode());

  // Ignore |is_resizing_| because it will be true in case of double tapping
  // (not double clicking) the divider without ever actually dragging it
  // anywhere. Double tapping the divider triggers StartResize(), EndResize(),
  // StartResize(), SwapWindows(), EndResize(). Double clicking the divider
  // (possible by using the emulator or chrome://flags/#force-tablet-mode)
  // triggers StartResize(), EndResize(), StartResize(), EndResize(),
  // SwapWindows(). Those two sequences of function calls are what were mainly
  // considered in writing the condition for bailing out here, to disallow
  // swapping windows when the divider is being dragged or is animating.
  if (IsDividerAnimating())
    return;

  aura::Window* new_left_window = secondary_window_;
  aura::Window* new_right_window = primary_window_;
  primary_window_ = new_left_window;
  // If there is a window in the snap position, trigger a WMEvent to snap it in
  // the opposite position. If there is no window, i.e. Overview is open,
  // `UpdateStateAndNotifyObservers()` will update the bounds themselves.
  if (IsSnapped(primary_window_)) {
    auto* window_state = WindowState::Get(primary_window_);
    const WMEvent primary_window_event(
        WM_EVENT_SNAP_PRIMARY,
        window_state->snap_ratio().value_or(kDefaultSnapRatio));
    window_state->OnWMEvent(&primary_window_event);
  }
  secondary_window_ = new_right_window;
  if (IsSnapped(secondary_window_)) {
    auto* window_state = WindowState::Get(secondary_window_);
    const WMEvent secondary_window_event(
        WM_EVENT_SNAP_SECONDARY,
        window_state->snap_ratio().value_or(kDefaultSnapRatio));
    window_state->OnWMEvent(&secondary_window_event);
  }

  // Update |default_snap_position_| if necessary.
  if (!primary_window_ || !secondary_window_)
    default_snap_position_ =
        primary_window_ ? SnapPosition::kPrimary : SnapPosition::kSecondary;

  divider_position_ = GetClosestFixedDividerPosition();
  UpdateStateAndNotifyObservers();
  NotifyWindowSwapped();

  base::RecordAction(
      base::UserMetricsAction("SplitView_DoubleTapDividerSwapWindows"));
}

SplitViewController::SnapPosition
SplitViewController::GetPositionOfSnappedWindow(
    const aura::Window* window) const {
  DCHECK(IsWindowInSplitView(window));
  return window == primary_window_ ? SnapPosition::kPrimary
                                   : SnapPosition::kSecondary;
}

aura::Window* SplitViewController::GetSnappedWindow(SnapPosition position) {
  DCHECK_NE(SnapPosition::kNone, position);
  return position == SnapPosition::kPrimary ? primary_window_
                                            : secondary_window_;
}

aura::Window* SplitViewController::GetDefaultSnappedWindow() {
  if (default_snap_position_ == SnapPosition::kPrimary)
    return primary_window_;
  if (default_snap_position_ == SnapPosition::kSecondary)
    return secondary_window_;
  return nullptr;
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInParent(
    SnapPosition snap_position,
    aura::Window* window_for_minimum_size,
    float snap_ratio) {
  gfx::Rect bounds = GetSnappedWindowBoundsInScreen(
      snap_position, window_for_minimum_size, snap_ratio);
  wm::ConvertRectFromScreen(root_window_, &bounds);
  return bounds;
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInParent(
    SnapPosition snap_position,
    aura::Window* window_for_minimum_size) {
  return GetSnappedWindowBoundsInParent(snap_position, window_for_minimum_size,
                                        kDefaultSnapRatio);
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInScreen(
    SnapPosition snap_position,
    aura::Window* window_for_minimum_size,
    float snap_ratio) {
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  if (snap_position == SnapPosition::kNone)
    return work_area_bounds_in_screen;

  if (window_for_minimum_size && ShouldUseWindowBoundsDuringFastResize()) {
    gfx::Rect bounds = window_for_minimum_size->bounds();
    wm::ConvertRectToScreen(window_for_minimum_size->parent(), &bounds);
    return bounds;
  }
  const bool horizontal = IsLayoutHorizontal(root_window_);
  const bool snap_left_or_top =
      IsPhysicalLeftOrTop(snap_position, root_window_);

  // TODO(crbug.com/1231308): Clean-up: make sure only tablet mode uses
  // SplitViewController and migrate
  // `SplitViewController::GetSnappedWindowBoundsInScreen()` calls in clamshell
  // mode to `GetSnappedWindowBounds()` in window_positioning_utils.cc.
  const bool in_tablet = Shell::Get()->tablet_mode_controller()->InTabletMode();
  const int work_area_size = GetDividerEndPosition();
  int divider_position = divider_position_ < 0
                             ? GetDividerPosition(snap_position, snap_ratio)
                             : divider_position_;

  // Edit `divider_position` if window restore is currently restoring a snapped
  // window; take into account the snap percentage saved by the window. Only do
  // this for clamshell mode; in tablet mode we are OK with restoring to the
  // default half snap state.
  if (divider_position_ < 0 && !in_tablet) {
    if (auto* window = WindowRestoreController::Get()->to_be_snapped_window()) {
      app_restore::WindowInfo* window_info =
          window->GetProperty(app_restore::kWindowInfoKey);
      if (window_info && window_info->snap_percentage) {
        const int snap_percentage = *window_info->snap_percentage;
        divider_position = snap_percentage * work_area_size / 100;
        if (!snap_left_or_top)
          divider_position = work_area_size - divider_position;
      }
    }
  }

  int window_size;
  if (snap_left_or_top) {
    window_size = divider_position;
  } else {
    window_size = work_area_size - divider_position;
    // In tablet mode, there is a divider widget of which |divider_position|
    // refers to the left or top, and so we should subtract the thickness.
    if (in_tablet)
      window_size -= kSplitviewDividerShortSideLength;
  }

  const int minimum =
      GetMinimumWindowLength(window_for_minimum_size, horizontal);
  DCHECK(window_for_minimum_size || minimum == 0);
  if (window_size < minimum) {
    if (in_tablet && !is_resizing_) {
      // If |window_for_minimum_size| really gets snapped, then the divider will
      // be adjusted to its default position. Compute |window_size| accordingly.
      window_size = work_area_size / 2 - kSplitviewDividerShortSideLength / 2;
      // If |work_area_size| is odd, then the default divider position is
      // rounded down, toward the left or top, but then if |snap_left_or_top| is
      // false, that means |window_size| should now be rounded up.
      if (!snap_left_or_top && work_area_size % 2 == 1)
        ++window_size;
    } else {
      window_size = minimum;
    }
  }

  if (window_for_minimum_size && !in_tablet) {
    // Apply the unresizable snapping constraint to the snapped bounds if we're
    // in the clamshell mode.
    const gfx::Size* preferred_size =
        window_for_minimum_size->GetProperty(kUnresizableSnappedSizeKey);
    if (preferred_size &&
        !WindowState::Get(window_for_minimum_size)->CanResize()) {
      if (horizontal && preferred_size->width() > 0)
        window_size = preferred_size->width();
      if (!horizontal && preferred_size->height() > 0)
        window_size = preferred_size->height();
    }
  }

  // Get the parameter values for which |gfx::Rect::SetByBounds| would recreate
  // |work_area_bounds_in_screen|.
  int left = work_area_bounds_in_screen.x();
  int top = work_area_bounds_in_screen.y();
  int right = work_area_bounds_in_screen.right();
  int bottom = work_area_bounds_in_screen.bottom();

  // Make |snapped_window_bounds_in_screen| by modifying one of the above four
  // values: the one that represents the inner edge of the snapped bounds.
  int& left_or_top = horizontal ? left : top;
  int& right_or_bottom = horizontal ? right : bottom;
  if (snap_left_or_top)
    right_or_bottom = left_or_top + window_size;
  else
    left_or_top = right_or_bottom - window_size;

  gfx::Rect snapped_window_bounds_in_screen;
  snapped_window_bounds_in_screen.SetByBounds(left, top, right, bottom);
  return snapped_window_bounds_in_screen;
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInScreen(
    SnapPosition snap_position,
    aura::Window* window_for_minimum_size) {
  return GetSnappedWindowBoundsInScreen(snap_position, window_for_minimum_size,
                                        kDefaultSnapRatio);
}

bool SplitViewController::ShouldUseWindowBoundsDuringFastResize() {
  return is_resizing_ && tablet_resize_mode_ == TabletResizeMode::kFast;
}

int SplitViewController::GetDefaultDividerPosition() const {
  return GetDividerPosition(SnapPosition::kPrimary, kDefaultSnapRatio);
}

int SplitViewController::GetDividerPosition(SnapPosition snap_position,
                                            float snap_ratio) const {
  int divider_end_position = GetDividerEndPosition();
  // `snap_width` needs to be a float so that the rounding is performed at the
  // end of the computation of `next_divider_position`. It's important because a
  // 1-DIP gap between snapped windows precludes multiresizing. See b/262011280.
  const float snap_width = divider_end_position * snap_ratio;
  int next_divider_position = snap_position == SnapPosition::kPrimary
                                  ? snap_width
                                  : divider_end_position - snap_width;
  if (split_view_type_ == SplitViewType::kTabletType)
    next_divider_position -= kSplitviewDividerShortSideLength / 2;
  return next_divider_position;
}

bool SplitViewController::IsDividerAnimating() const {
  return divider_snap_animation_ && divider_snap_animation_->is_animating();
}

void SplitViewController::StartResize(const gfx::Point& location_in_screen) {
  DCHECK(InSplitViewMode());

  // |is_resizing_| may be true here, because you can start dragging the divider
  // with a pointing device while already dragging it by touch, or vice versa.
  // It is possible by using the emulator or chrome://flags/#force-tablet-mode.
  // Bailing out here does not stop the user from dragging by touch and with a
  // pointing device simultaneously; it just avoids duplicate calls to
  // CreateDragDetails() and OnDragStarted(). We also bail out here if you try
  // to start dragging the divider during its snap animation.
  if (is_resizing_ || IsDividerAnimating()) {
    return;
  }

  is_resizing_ = true;
  split_view_divider_->UpdateDividerBounds();
  previous_event_location_ = location_in_screen;

  accumulated_drag_time_ticks_ = base::TimeTicks::Now();
  accumulated_drag_distance_ = 0;

  tablet_resize_mode_ = TabletResizeMode::kNormal;

  for (auto* window : {primary_window_, secondary_window_}) {
    if (window == nullptr)
      continue;
    WindowState* window_state = WindowState::Get(window);
    gfx::Point location_in_parent(location_in_screen);
    ::wm::ConvertPointFromScreen(window->parent(), &location_in_parent);
    int window_component = GetWindowComponentForResize(window);
    window_state->CreateDragDetails(gfx::PointF(location_in_parent),
                                    window_component,
                                    ::wm::WINDOW_MOVE_SOURCE_TOUCH);
    window_state->OnDragStarted(window_component);
  }

  base::RecordAction(base::UserMetricsAction("SplitView_ResizeWindows"));
  if (state_ == State::kBothSnapped) {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        split_view_divider_->divider_widget()->GetCompositor(),
        kTabletSplitViewResizeMultiHistogram,
        kTabletSplitViewResizeMultiMaxLatencyHistogram);
    return;
  }
  DCHECK(GetOverviewSession());
  if (GetOverviewSession()->GetGridWithRootWindow(root_window_)->empty()) {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        split_view_divider_->divider_widget()->GetCompositor(),
        kTabletSplitViewResizeSingleHistogram,
        kTabletSplitViewResizeSingleMaxLatencyHistogram);
  } else {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        split_view_divider_->divider_widget()->GetCompositor(),
        kTabletSplitViewResizeWithOverviewHistogram,
        kTabletSplitViewResizeWithOverviewMaxLatencyHistogram);
  }
}

void SplitViewController::Resize(const gfx::Point& location_in_screen) {
  DCHECK(InSplitViewMode());

  if (!is_resizing_)
    return;

  base::AutoReset<bool> auto_reset(&processing_resize_event_, true);

  presentation_time_recorder_->RequestNext();
  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  gfx::Point modified_location_in_screen =
      GetBoundedPosition(location_in_screen, work_area_bounds);

  // This updates `tablet_resize_mode_` based on drag speed.
  UpdateTabletResizeMode(base::TimeTicks::Now(), modified_location_in_screen);

  // If we are in the fast mode, start a timer that automatically invokes
  // `Resize()` after a timeout. This ensure that we can switch back to the
  // normal mode if the user stops dragging. Note: if the timer is already
  // active, this will simply move the deadline forward.
  if (tablet_resize_mode_ == TabletResizeMode::kFast) {
    resize_timer_.Start(FROM_HERE, kSplitViewChunkTime, this,
                        &SplitViewController::OnResizeTimer);
  }

  // Update |divider_position_|.
  UpdateDividerPosition(modified_location_in_screen);
  NotifyDividerPositionChanged();

  // Update the snapped window/windows and divider's position.
  UpdateSnappedWindowsAndDividerBounds();

  // Update the resize backdrop, as well as the black scrim layer's bounds and
  // opacity.
  UpdateResizeBackdrop();
  UpdateBlackScrim(modified_location_in_screen);

  // Apply window transform if necessary.
  SetWindowsTransformDuringResizing();

  previous_event_location_ = modified_location_in_screen;
}

void SplitViewController::EndResize(const gfx::Point& location_in_screen) {
  presentation_time_recorder_.reset();
  DCHECK(InSplitViewMode());
  if (!is_resizing_)
    return;
  // TODO(xdai): Use fade out animation instead of just removing it.
  black_scrim_layer_.reset();

  resize_timer_.Stop();
  tablet_resize_mode_ = TabletResizeMode::kNormal;
  is_resizing_ = false;

  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  gfx::Point modified_location_in_screen =
      GetBoundedPosition(location_in_screen, work_area_bounds);
  UpdateDividerPosition(modified_location_in_screen);
  NotifyDividerPositionChanged();
  // Need to update snapped windows bounds even if the split view mode may have
  // to exit. Otherwise it's possible for a snapped window stuck in the edge of
  // of the screen while overview mode is active.
  UpdateSnappedWindowsAndDividerBounds();
  NotifyWindowResized();

  const int target_divider_position = GetClosestFixedDividerPosition();
  if (divider_position_ == target_divider_position) {
    EndResizeImpl();
    EndTabletSplitViewAfterResizingIfAppropriate();
  } else {
    divider_snap_animation_ = std::make_unique<DividerSnapAnimation>(
        this, divider_position_, target_divider_position);
    divider_snap_animation_->Show();
  }
}

void SplitViewController::EndSplitView(EndReason end_reason) {
  if (!InSplitViewMode())
    return;

  end_reason_ = end_reason;

  // If we are currently in a resize but split view is ending, make sure to end
  // the resize. This can happen, for example, on the transition back to
  // clamshell mode or when a task is minimized during a resize. Likewise, if
  // split view is ending during the divider snap animation, then clean that up.
  // But if the split view is ending due to the destroy of `root_window_`, we
  // should skip the resize.
  const bool is_divider_animating = IsDividerAnimating();
  if ((is_resizing_ || is_divider_animating) &&
      end_reason != EndReason::kRootWindowDestroyed) {
    is_resizing_ = false;
    if (is_divider_animating) {
      // Don't call StopAndShoveAnimatedDivider as it will call observers.
      divider_snap_animation_->Stop();
      divider_position_ = divider_snap_animation_->ending_position();
    }
    EndResizeImpl();
  }

  // There is at least one case where this line of code is needed: if the user
  // presses Ctrl+W while resizing a clamshell split view window.
  presentation_time_recorder_.reset();

  // Remove observers when the split view mode ends.
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
  if (features::IsAdjustSplitViewForVKEnabled()) {
    keyboard::KeyboardUIController::Get()->RemoveObserver(this);
    Shell::Get()->activation_client()->RemoveObserver(this);
  }

  auto_snap_controller_.reset();

  StopObserving(SnapPosition::kPrimary);
  StopObserving(SnapPosition::kSecondary);
  black_scrim_layer_.reset();
  default_snap_position_ = SnapPosition::kNone;
  divider_position_ = -1;
  divider_closest_ratio_ = std::numeric_limits<float>::quiet_NaN();
  snapping_window_transformed_bounds_map_.clear();

  UpdateStateAndNotifyObservers();
  // Close splitview divider widget after updating state so that
  // OnDisplayMetricsChanged triggered by the widget closing correctly
  // finds out !InSplitViewMode().
  split_view_divider_.reset();
  base::RecordAction(base::UserMetricsAction("SplitView_EndSplitView"));
  const base::Time now = base::Time::Now();
  UMA_HISTOGRAM_LONG_TIMES("Ash.SplitView.TimeInSplitView",
                           now - splitview_start_time_);
  // We just ended split view on |root_window_|. If there is exactly one root
  // where split view is still active, then multi-display split view ends now.
  if (IsExactlyOneRootInSplitView()) {
    UMA_HISTOGRAM_LONG_TIMES("Ash.SplitView.TimeInMultiDisplaySplitView",
                             now - g_multi_display_split_view_start_time);
  }
}

bool SplitViewController::IsWindowInSplitView(
    const aura::Window* window) const {
  return window && (window == primary_window_ || window == secondary_window_);
}

void SplitViewController::InitDividerPositionForTransition(
    int divider_position) {
  // This should only be called before the actual carry-over happens.
  DCHECK(!InSplitViewMode());
  DCHECK_EQ(divider_position_, -1);
  divider_position_ = divider_position;
}

bool SplitViewController::IsWindowInTransitionalState(
    const aura::Window* window) const {
  return to_be_snapped_windows_observer_->IsObserving(window);
}

void SplitViewController::OnOverviewButtonTrayLongPressed(
    const gfx::Point& event_location) {
  // Do nothing if split view is not enabled.
  if (!ShouldAllowSplitView())
    return;

  // If in split view: The active snapped window becomes maximized. If overview
  // was seen alongside a snapped window, then overview mode ends.
  //
  // Otherwise: Enter split view iff the cycle list has at least one window, and
  // the first one is snappable.

  MruWindowTracker::WindowList mru_window_list =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);
  // Do nothing if there is one or less windows in the MRU list.
  if (mru_window_list.empty())
    return;

  auto* overview_controller = Shell::Get()->overview_controller();
  aura::Window* target_window = mru_window_list[0];

  // Exit split view mode if we are already in it.
  if (InSplitViewMode()) {
    DCHECK(IsWindowInSplitView(target_window));
    DCHECK(target_window);
    EndSplitView();
    overview_controller->EndOverview(
        OverviewEndAction::kOverviewButtonLongPress);
    MaximizeIfSnapped(target_window);
    wm::ActivateWindow(target_window);
    base::RecordAction(
        base::UserMetricsAction("Tablet_LongPressOverviewButtonExitSplitView"));
    return;
  }

  // Show a toast if the window cannot be snapped.
  if (!CanSnapWindow(target_window)) {
    ShowAppCannotSnapToast();
    return;
  }

  // Start overview mode if we aren't already in it.
  overview_controller->StartOverview(
      OverviewStartAction::kOverviewButtonLongPress,
      OverviewEnterExitType::kImmediateEnter);

  WindowState::Get(target_window)
      ->set_snap_action_source(
          WindowSnapActionSource::kLongPressOverviewButtonToSnap);
  SnapWindow(target_window, SnapPosition::kPrimary,
             /*activate_window=*/true);
  base::RecordAction(
      base::UserMetricsAction("Tablet_LongPressOverviewButtonEnterSplitView"));
}

void SplitViewController::OnWindowDragStarted(aura::Window* dragged_window) {
  DCHECK(dragged_window);
  if (IsWindowInSplitView(dragged_window)) {
    OnSnappedWindowDetached(dragged_window,
                            WindowDetachedReason::kWindowDragged);
  }

  // OnSnappedWindowDetached() may end split view mode.
  if (split_view_divider_)
    split_view_divider_->OnWindowDragStarted();
}

void SplitViewController::OnWindowDragEnded(
    aura::Window* dragged_window,
    SnapPosition desired_snap_position,
    const gfx::Point& last_location_in_screen) {
  if (window_util::IsDraggingTabs(dragged_window)) {
    dragged_window_observer_ = std::make_unique<TabDraggedWindowObserver>(
        this, dragged_window, desired_snap_position, last_location_in_screen);
  } else {
    EndWindowDragImpl(dragged_window, dragged_window->is_destroying(),
                      desired_snap_position, last_location_in_screen);
  }
}

void SplitViewController::OnWindowDragCanceled() {
  if (split_view_divider_)
    split_view_divider_->OnWindowDragEnded();
}

SplitViewController::SnapPosition SplitViewController::ComputeSnapPosition(
    const gfx::Point& last_location_in_screen) {
  const int divider_position = InSplitViewMode() ? this->divider_position()
                                                 : GetDefaultDividerPosition();
  const int position = IsLayoutHorizontal(root_window_)
                           ? last_location_in_screen.x()
                           : last_location_in_screen.y();
  return (position <= divider_position) == IsLayoutPrimary(root_window_)
             ? SnapPosition::kPrimary
             : SnapPosition::kSecondary;
}

bool SplitViewController::BoundsChangeIsFromVKAndAllowed(
    aura::Window* window) const {
  // Make sure that it is the bottom window who is requiring bounds change.
  return features::IsAdjustSplitViewForVKEnabled() && changing_bounds_by_vk_ &&
         window ==
             (IsLayoutPrimary(window) ? secondary_window_ : primary_window_);
}

void SplitViewController::AddObserver(SplitViewObserver* observer) {
  observers_.AddObserver(observer);
}

void SplitViewController::RemoveObserver(SplitViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SplitViewController::OnWindowPropertyChanged(aura::Window* window,
                                                  const void* key,
                                                  intptr_t old) {
  // If the window's resizibility property changes (must be from resizable ->
  // unresizable), end the split view mode and also end overview mode if
  // overview mode is active at the moment.
  if (key != aura::client::kResizeBehaviorKey)
    return;

  // It is possible the property gets updated and is still the same value.
  if (window->GetProperty(aura::client::kResizeBehaviorKey) ==
      static_cast<int>(old)) {
    return;
  }

  if (CanSnapWindow(window))
    return;

  EndSplitView();
  Shell::Get()->overview_controller()->EndOverview(
      OverviewEndAction::kSplitView);
  ShowAppCannotSnapToast();
}

void SplitViewController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  DCHECK_EQ(root_window_, window->GetRootWindow());

  if (InTabletSplitViewMode() && is_resizing_) {
    // Bounds may be changed while we are processing a resize event. In this
    // case, we don't update the windows transform here, since it will be done
    // soon anyway. If we are *not* currently processing a resize, it means the
    // bounds of a window have been updated "async", and we need to update the
    // window's transform.
    if (!processing_resize_event_)
      SetWindowsTransformDuringResizing();
    return;
  }

  if (!InClamshellSplitViewMode())
    return;

  WindowState* window_state = WindowState::Get(window);
  if (window_state->is_dragged()) {
    DCHECK_NE(WindowResizer::kBoundsChange_None,
              window_state->drag_details()->bounds_change);
    if (window_state->drag_details()->bounds_change ==
        WindowResizer::kBoundsChange_Repositions) {
      // Ending overview will also end clamshell split view.
      Shell::Get()->overview_controller()->EndOverview(
          OverviewEndAction::kSplitView);
      return;
    }
    DCHECK(window_state->drag_details()->bounds_change &
           WindowResizer::kBoundsChange_Resizes);
    DCHECK(presentation_time_recorder_);
    presentation_time_recorder_->RequestNext();
  }

  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);

  if (IsLayoutHorizontal(window)) {
    divider_position_ = window == primary_window_
                            ? new_bounds.width()
                            : work_area.width() - new_bounds.width();
  } else {
    divider_position_ = window == primary_window_
                            ? new_bounds.height()
                            : work_area.height() - new_bounds.height();
  }
  NotifyDividerPositionChanged();
}

void SplitViewController::OnWindowDestroyed(aura::Window* window) {
  DCHECK(InSplitViewMode());
  DCHECK(IsWindowInSplitView(window));
  auto iter = snapping_window_transformed_bounds_map_.find(window);
  if (iter != snapping_window_transformed_bounds_map_.end())
    snapping_window_transformed_bounds_map_.erase(iter);
  OnSnappedWindowDetached(window, WindowDetachedReason::kWindowDestroyed);
  if (to_be_activated_window_ == window)
    to_be_activated_window_ = nullptr;
}

void SplitViewController::OnResizeLoopStarted(aura::Window* window) {
  if (!InClamshellSplitViewMode())
    return;

  // In clamshell mode, if splitview is active (which means overview is active
  // at the same time), only the resize that happens on the window edge that's
  // next to the overview grid will resize the window and overview grid at the
  // same time. For the resize that happens on the other part of the window,
  // we'll just end splitview and overview mode.
  if (WindowState::Get(window)->drag_details()->window_component !=
      GetWindowComponentForResize(window)) {
    // Ending overview will also end clamshell split view.
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEndAction::kSplitView);
    return;
  }

  DCHECK_NE(State::kBothSnapped, state_);
  DCHECK(GetOverviewSession());
  if (GetOverviewSession()->GetGridWithRootWindow(root_window_)->empty()) {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        window->layer()->GetCompositor(),
        kClamshellSplitViewResizeSingleHistogram,
        kClamshellSplitViewResizeSingleMaxLatencyHistogram);
  } else {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        window->layer()->GetCompositor(),
        kClamshellSplitViewResizeWithOverviewHistogram,
        kClamshellSplitViewResizeWithOverviewMaxLatencyHistogram);
  }
}

void SplitViewController::OnResizeLoopEnded(aura::Window* window) {
  if (!InClamshellSplitViewMode())
    return;

  presentation_time_recorder_.reset();

  NotifyWindowResized();

  if (divider_position_ < GetDividerEndPosition() * kOneThirdSnapRatio ||
      divider_position_ > GetDividerEndPosition() * kTwoThirdSnapRatio) {
    // Ending overview will also end clamshell split view.
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEndAction::kSplitView);
    WindowState::Get(window)->Maximize();
  }
}

void SplitViewController::OnPostWindowStateTypeChange(
    WindowState* window_state,
    WindowStateType old_type) {
  DCHECK_EQ(
      window_state->GetDisplay().id(),
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window_).id());

  aura::Window* window = window_state->window();
  if (window_state->IsSnapped()) {
    bool do_divider_spawn_animation = false;
    // Only need to do divider spawn animation if split view is to be active,
    // window is not minimized and has an non-identify transform in tablet mode.
    // If window|is currently minimized then it will undergo the unminimizing
    // animation instead, therefore skip the divider spawn animation if
    // the window is minimized.
    if (state_ == State::kNoSnap &&
        split_view_type_ == SplitViewType::kTabletType &&
        old_type != WindowStateType::kMinimized &&
        !window->transform().IsIdentity()) {
      // For the divider spawn animation, at the end of the delay, the divider
      // shall be visually aligned with an edge of |window|. This effect will
      // be more easily achieved after |window| has been snapped and the
      // corresponding transform animation has begun. So for now, just set a
      // flag to indicate that the divider spawn animation should be done.
      do_divider_spawn_animation = true;
    }
    OnWindowSnapped(window, old_type);
    if (do_divider_spawn_animation)
      DoSplitDividerSpawnAnimation(window);
  } else if (window_state->IsNormalStateType() || window_state->IsMaximized() ||
             window_state->IsFullscreen()) {
    // End split view, and also overview if overview is active, in these cases:
    // 1. A left clamshell split view window gets unsnapped by Alt+[.
    // 2. A right clamshell split view window gets unsnapped by Alt+].
    // 3. A (clamshell or tablet) split view window gets maximized.
    // 4. A (clamshell or tablet) split view window becomes full screen.
    EndSplitView();
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEndAction::kSplitView);
  } else if (window_state->IsFloated()) {
    OnSnappedWindowDetached(window, WindowDetachedReason::kWindowFloated);

    // TODO(crbug.com/1351562): Consider ending overview here.
  } else if (window_state->IsMinimized()) {
    OnSnappedWindowDetached(window, WindowDetachedReason::kWindowMinimized);

    if (!InSplitViewMode()) {
      // We have different behaviors for a minimized window: in tablet splitview
      // mode, we'll insert the minimized window back to overview, as normally
      // the window is not supposed to be minmized in tablet mode. And in
      // clamshell splitview mode, we respect the minimization of the window
      // and end overview instead.
      if (split_view_type_ == SplitViewType::kTabletType) {
        InsertWindowToOverview(window);
      } else {
        Shell::Get()->overview_controller()->EndOverview(
            OverviewEndAction::kSplitView);
      }
    }
  }
}

void SplitViewController::OnPinnedStateChanged(aura::Window* pinned_window) {
  // Disable split view for pinned windows.
  if (WindowState::Get(pinned_window)->IsPinned() && InSplitViewMode())
    EndSplitView(EndReason::kUnsnappableWindowActivated);
}

void SplitViewController::OnOverviewModeStarting() {
  DCHECK(InSplitViewMode());

  // If split view mode is active, reset |state_| to make it be able to select
  // another window from overview window grid.
  if (default_snap_position_ == SnapPosition::kPrimary) {
    StopObserving(SnapPosition::kSecondary);
  } else if (default_snap_position_ == SnapPosition::kSecondary) {
    StopObserving(SnapPosition::kPrimary);
  }
  UpdateStateAndNotifyObservers();
}

void SplitViewController::OnOverviewModeEnding(
    OverviewSession* overview_session) {
  DCHECK(InSplitViewMode());

  // If overview is ended because of a window getting snapped, suppress the
  // overview exiting animation.
  if (state_ == State::kBothSnapped)
    overview_session->SetWindowListNotAnimatedWhenExiting(root_window_);

  // If clamshell split view mode is active, bail out. |OnOverviewModeEnded|
  // will end split view. We do not end split view here, because that would mess
  // up histograms of overview exit animation smoothness.
  if (split_view_type_ == SplitViewType::kClamshellType)
    return;

  // Tablet split view mode is active. If it still only has one snapped window,
  // snap the first snappable window in the overview grid on the other side.
  if (state_ == State::kBothSnapped)
    return;
  OverviewGrid* current_grid =
      overview_session->GetGridWithRootWindow(root_window_);
  if (!current_grid || current_grid->empty())
    return;
  for (const auto& overview_item : current_grid->window_list()) {
    aura::Window* window = overview_item->GetWindow();
    if (CanSnapWindow(window) && window != GetDefaultSnappedWindow()) {
      // Remove the overview item before snapping because the overview session
      // is unavailable to retrieve outside this function after OnOverviewEnding
      // is notified.
      overview_item->RestoreWindow(/*reset_transform=*/false);
      overview_session->RemoveItem(overview_item.get());

      WindowState::Get(window)->set_snap_action_source(
          WindowSnapActionSource::kAutoSnapBySplitview);
      SnapWindow(window, (default_snap_position_ == SnapPosition::kPrimary)
                             ? SnapPosition::kSecondary
                             : SnapPosition::kPrimary);
      // If ending overview causes a window to snap, also do not do exiting
      // overview animation.
      overview_session->SetWindowListNotAnimatedWhenExiting(root_window_);
      return;
    }
  }

  // The overview grid has at least one window, but has none that can be snapped
  // in split view. If overview is ending because of switching between virtual
  // desks, then there is no need to do anything here. Otherwise, end split view
  // and show the cannot snap toast.
  if (DesksController::Get()->AreDesksBeingModified())
    return;
  EndSplitView();
  ShowAppCannotSnapToast();
}

void SplitViewController::OnOverviewModeEnded() {
  DCHECK(InSplitViewMode());
  if (split_view_type_ == SplitViewType::kClamshellType)
    EndSplitView();
}

void SplitViewController::OnDisplayRemoved(
    const display::Display& old_display) {
  // Display removal always triggers a window activation which ends overview,
  // and therefore ends clamshell split view, before |OnDisplayRemoved| is
  // called. In clamshell mode, |OverviewController::CanEndOverview| always
  // returns true, meaning that overview is guaranteed to end successfully.
  DCHECK(!InClamshellSplitViewMode());

  // If the `root_window_`is the root window of the display which is going to
  // be removed, there's no need to start overview.
  if (GetRootWindowSettings(root_window_)->display_id ==
      display::kInvalidDisplayId) {
    return;
  }

  // If we are in tablet split view with only one snapped window, make sure we
  // are in overview (see https://crbug.com/1027179).
  if (state_ == State::kPrimarySnapped || state_ == State::kSecondarySnapped) {
    Shell::Get()->overview_controller()->StartOverview(
        OverviewStartAction::kSplitView,
        OverviewEnterExitType::kImmediateEnter);
  }
}

void SplitViewController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  // Avoid |ScreenAsh::GetDisplayNearestWindow|, which has a |DCHECK| that fails
  // if the display is being deleted. Use |GetRootWindowSettings| directly, and
  // if the display is being deleted, we will get |display::kInvalidDisplayId|.
  if (GetRootWindowSettings(root_window_)->display_id != display.id())
    return;

  // We need to update |is_previous_layout_right_side_up_| even if split view
  // mode is not active.
  const bool is_previous_layout_right_side_up =
      is_previous_layout_right_side_up_;
  is_previous_layout_right_side_up_ = IsLayoutPrimary(display);

  if (!InSplitViewMode())
    return;

  // If one of the snapped windows becomes unsnappable, end the split view mode
  // directly.
  if ((primary_window_ && !CanSnapWindow(primary_window_)) ||
      (secondary_window_ && !CanSnapWindow(secondary_window_))) {
    if (!Shell::Get()->session_controller()->IsUserSessionBlocked())
      EndSplitView();
    return;
  }

  // In clamshell split view mode, the divider position will be adjusted in
  // |OnWindowBoundsChanged|.
  if (split_view_type_ == SplitViewType::kClamshellType)
    return;

  // Before adjusting the divider position for the new display metrics, if the
  // divider is animating to a snap position, then stop it and shove it there.
  // Postpone EndTabletSplitViewAfterResizingIfAppropriate() until after the
  // adjustment, because the new display metrics will be used to compare the
  // divider position against the edges of the screen.
  if (IsDividerAnimating()) {
    StopAndShoveAnimatedDivider();
    EndResizeImpl();
  }

  if ((metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION) ||
      (metrics & display::DisplayObserver::DISPLAY_METRIC_WORK_AREA)) {
    // Set default |divider_closest_ratio_| to kFixedPositionRatios[1].
    if (std::isnan(divider_closest_ratio_))
      divider_closest_ratio_ = kFixedPositionRatios[1];

    // Reverse the position ratio if top/left window changes.
    if (is_previous_layout_right_side_up != IsLayoutPrimary(display))
      divider_closest_ratio_ = 1.f - divider_closest_ratio_;
    divider_position_ =
        static_cast<int>(divider_closest_ratio_ * GetDividerEndPosition()) -
        kSplitviewDividerShortSideLength / 2;
  }

  // For other display configuration changes, we only move the divider to the
  // closest fixed position.
  if (!is_resizing_)
    divider_position_ = GetClosestFixedDividerPosition();

  EndTabletSplitViewAfterResizingIfAppropriate();
  if (!InSplitViewMode())
    return;

  NotifyDividerPositionChanged();
  UpdateSnappedWindowsAndDividerBounds();
}

void SplitViewController::OnTabletModeStarting() {
  split_view_type_ = SplitViewType::kTabletType;
}

void SplitViewController::OnTabletModeStarted() {
  is_previous_layout_right_side_up_ = IsCurrentScreenOrientationPrimary();
  // If splitview is active when tablet mode is starting, do the clamshell mode
  // splitview to tablet mode splitview transition by adding the split view
  // divider bar and also adjust the |divider_position_| so that it's on one of
  // the three fixed positions.
  if (InSplitViewMode()) {
    divider_position_ = GetClosestFixedDividerPosition();
    split_view_divider_ = std::make_unique<SplitViewDivider>(this);
    UpdateSnappedWindowsAndDividerBounds();
    NotifyDividerPositionChanged();
  }
}

void SplitViewController::OnTabletModeEnding() {
  split_view_type_ = SplitViewType::kClamshellType;

  // There is no divider in clamshell split view.
  const bool is_divider_animating = IsDividerAnimating();
  if (is_resizing_ || is_divider_animating) {
    is_resizing_ = false;
    if (is_divider_animating)
      StopAndShoveAnimatedDivider();
    EndResizeImpl();
  }
  split_view_divider_.reset();
}

void SplitViewController::OnTabletModeEnded() {
  is_previous_layout_right_side_up_ = true;
}

void SplitViewController::OnAccessibilityStatusChanged() {
  // TODO(crubg.com/853588): Exit split screen if ChromeVox is turned on until
  // they are compatible.
  if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled())
    EndSplitView();
}

void SplitViewController::OnAccessibilityControllerShutdown() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

void SplitViewController::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& screen_bounds) {
  if (!features::IsAdjustSplitViewForVKEnabled())
    return;

  // The window only needs to be moved if it is in the portrait mode.
  if (IsLayoutHorizontal(root_window_))
    return;

  // We only modify the bottom window if there is one and the current active
  // input field is in the bottom window.
  aura::Window* bottom_window = GetPhysicalRightOrBottomWindow();
  if (!bottom_window &&
      !bottom_window->Contains(window_util::GetActiveWindow())) {
    return;
  }

  // If the virtual keyboard is disabled, restore to original layout.
  if (screen_bounds.IsEmpty()) {
    UpdateSnappedWindowsAndDividerBounds();
    return;
  }

  // Get current active input field.
  auto* text_input_client = GetCurrentInputMethod()->GetTextInputClient();
  if (!text_input_client)
    return;

  // Get caret bounds.
  const gfx::Rect caret_bounds = text_input_client->GetCaretBounds();
  if (caret_bounds == gfx::Rect())
    return;

  // Move the bottom window if the caret is less than `kMinCaretKeyboardDist`
  // dip above the upper bounds of the virtual keyboard.
  const int keyboard_occluded_y = screen_bounds.y();
  if (keyboard_occluded_y - caret_bounds.bottom() > kMinCaretKeyboardDist)
    return;

  // Move bottom window above the virtual keyboard but the upper bounds cannot
  // exceeds `kMinDividerPositionRatio` of the screen height.
  gfx::Rect bottom_bounds = bottom_window->GetBoundsInScreen();
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  const int y =
      std::max(keyboard_occluded_y - bottom_bounds.height(),
               static_cast<int>(work_area.y() +
                                work_area.height() * kMinDividerPositionRatio));
  bottom_bounds.set_y(y);
  bottom_bounds.set_height(keyboard_occluded_y - y);

  int divider_position = y - kSplitviewDividerShortSideLength;

  // Set bottom window bounds.
  {
    base::AutoReset<bool> enable_bounds_change(&changing_bounds_by_vk_, true);
    bottom_window->SetBoundsInScreen(
        bottom_bounds,
        display::Screen::GetScreen()->GetDisplayNearestWindow(root_window_));
  }

  // Set split view divider bounds.
  split_view_divider_->divider_widget()->SetBounds(
      SplitViewDivider::GetDividerBoundsInScreen(work_area, /*landscape=*/false,
                                                 divider_position,
                                                 /*is_dragging=*/false));
  // Make split view divider unadjustable.
  split_view_divider_->SetAdjustable(false);
}

void SplitViewController::OnWindowActivated(ActivationReason reason,
                                            aura::Window* gained_active,
                                            aura::Window* lost_active) {
  if (!features::IsAdjustSplitViewForVKEnabled())
    return;

  // If the bottom window is moved for the virtual keyboard (the split view
  // divider bar is unadjustable), when the bottom window lost active, restore
  // to the original layout.
  if (!split_view_divider_ || split_view_divider_->IsAdjustable())
    return;

  // It should be in portrait mode.
  if (IsLayoutHorizontal(root_window_))
    return;

  aura::Window* bottom_window = GetPhysicalRightOrBottomWindow();
  if (!bottom_window)
    return;

  if (bottom_window->Contains(lost_active) &&
      !bottom_window->Contains(gained_active)) {
    UpdateSnappedWindowsAndDividerBounds();
  }
}

aura::Window* SplitViewController::GetPhysicalLeftOrTopWindow() {
  DCHECK(root_window_);
  return IsLayoutPrimary(root_window_) ? primary_window_ : secondary_window_;
}

aura::Window* SplitViewController::GetPhysicalRightOrBottomWindow() {
  DCHECK(root_window_);
  return IsLayoutPrimary(root_window_) ? secondary_window_ : primary_window_;
}

void SplitViewController::StartObserving(aura::Window* window) {
  if (window && !window->HasObserver(this)) {
    Shell::Get()->shadow_controller()->UpdateShadowForWindow(window);
    window->AddObserver(this);
    WindowState::Get(window)->AddObserver(this);
    if (split_view_divider_)
      split_view_divider_->AddObservedWindow(window);
  }
}

void SplitViewController::StopObserving(SnapPosition snap_position) {
  aura::Window* window = GetSnappedWindow(snap_position);
  if (window == primary_window_)
    primary_window_ = nullptr;
  else
    secondary_window_ = nullptr;

  if (window && window->HasObserver(this)) {
    window->RemoveObserver(this);
    WindowState::Get(window)->RemoveObserver(this);
    if (split_view_divider_)
      split_view_divider_->RemoveObservedWindow(window);
    Shell::Get()->shadow_controller()->UpdateShadowForWindow(window);

    // It's possible that when we try to snap an ARC app window, while we are
    // waiting for its state/bounds to the expected state/bounds, another window
    // snap request comes in and causing the previous to-be-snapped window to
    // be un-observed, in this case we should restore the previous to-be-snapped
    // window's transform if it's unidentity.
    RestoreTransformIfApplicable(window);
  }
}

void SplitViewController::UpdateStateAndNotifyObservers() {
  State previous_state = state_;
  if (IsSnapped(primary_window_) && IsSnapped(secondary_window_))
    state_ = State::kBothSnapped;
  else if (IsSnapped(primary_window_))
    state_ = State::kPrimarySnapped;
  else if (IsSnapped(secondary_window_))
    state_ = State::kSecondarySnapped;
  else
    state_ = State::kNoSnap;

  // We still notify observers even if |state_| doesn't change as it's possible
  // to snap a window to a position that already has a snapped window. However,
  // |previous_state| and |state_| cannot both be |State::kNoSnap|.
  // When |previous_state| is |State::kNoSnap|, it indicates to
  // observers that split view mode started. Likewise, when |state_| is
  // |State::kNoSnap|, it indicates to observers that split view mode
  // ended.
  DCHECK(previous_state != State::kNoSnap || state_ != State::kNoSnap);
  for (auto& observer : observers_)
    observer.OnSplitViewStateChanged(previous_state, state_);
}

void SplitViewController::NotifyDividerPositionChanged() {
  for (auto& observer : observers_)
    observer.OnSplitViewDividerPositionChanged();
}

void SplitViewController::NotifyWindowResized() {
  for (auto& observer : observers_)
    observer.OnSplitViewWindowResized();
}

void SplitViewController::NotifyWindowSwapped() {
  for (auto& observer : observers_)
    observer.OnSplitViewWindowSwapped();
}

void SplitViewController::UpdateBlackScrim(
    const gfx::Point& location_in_screen) {
  DCHECK(InSplitViewMode());

  if (!black_scrim_layer_) {
    // Create an invisible black scrim layer.
    black_scrim_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
    black_scrim_layer_->SetColor(AshColorProvider::Get()->GetBackgroundColor());
    // Set the black scrim layer underneath split view divider.
    auto* divider_layer =
        split_view_divider_->divider_widget()->GetNativeWindow()->layer();
    auto* divider_parent_layer = divider_layer->parent();
    divider_parent_layer->Add(black_scrim_layer_.get());
    divider_parent_layer->StackBelow(black_scrim_layer_.get(), divider_layer);
  }

  // Decide where the black scrim should show and update its bounds.
  SnapPosition position = GetBlackScrimPosition(location_in_screen);
  if (position == SnapPosition::kNone) {
    black_scrim_layer_.reset();
    return;
  }
  black_scrim_layer_->SetBounds(GetSnappedWindowBoundsInScreen(
      position, /*window_for_minimum_size=*/nullptr));

  // Update its opacity. The opacity increases as it gets closer to the edge of
  // the screen.
  const int location = IsLayoutHorizontal(root_window_)
                           ? location_in_screen.x()
                           : location_in_screen.y();
  gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  if (!IsLayoutHorizontal(root_window_))
    work_area_bounds.Transpose();
  float opacity = kBlackScrimOpacity;
  const float ratio = kOneThirdSnapRatio - kBlackScrimFadeInRatio;
  const int distance = std::min(std::abs(location - work_area_bounds.x()),
                                std::abs(work_area_bounds.right() - location));
  if (distance > work_area_bounds.width() * ratio) {
    opacity -= kBlackScrimOpacity *
               (distance - work_area_bounds.width() * ratio) /
               (work_area_bounds.width() * kBlackScrimFadeInRatio);
    opacity = std::max(opacity, 0.f);
  }
  black_scrim_layer_->SetOpacity(opacity);
}

void SplitViewController::UpdateResizeBackdrop() {
  // Creates a backdrop layer. It is stacked below the snapped window.
  auto create_backdrop = [](aura::Window* window) {
    auto resize_backdrop_layer =
        std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);

    ui::Layer* parent = window->layer()->parent();
    ui::Layer* stacking_target = window->layer();
    parent->Add(resize_backdrop_layer.get());
    parent->StackBelow(resize_backdrop_layer.get(), stacking_target);

    return resize_backdrop_layer;
  };

  // Updates the bounds and color of a backdrop.
  auto update_backdrop = [this](SnapPosition position, aura::Window* window,
                                ui::Layer* backdrop) {
    backdrop->SetBounds(GetSnappedWindowBoundsInParent(position, nullptr));
    backdrop->SetColor(window->GetProperty(
        wm::IsActiveWindow(window) ? chromeos::kFrameActiveColorKey
                                   : chromeos::kFrameInactiveColorKey));
  };

  if (state_ == State::kPrimarySnapped || state_ == State::kBothSnapped) {
    if (!left_resize_backdrop_layer_)
      left_resize_backdrop_layer_ = create_backdrop(primary_window_);
    update_backdrop(SnapPosition::kPrimary, primary_window_,
                    left_resize_backdrop_layer_.get());
  }
  if (state_ == State::kSecondarySnapped || state_ == State::kBothSnapped) {
    if (!right_resize_backdrop_layer_)
      right_resize_backdrop_layer_ = create_backdrop(secondary_window_);
    update_backdrop(SnapPosition::kSecondary, secondary_window_,
                    right_resize_backdrop_layer_.get());
  }
}

void SplitViewController::UpdateSnappedWindowsAndDividerBounds() {
  // Update the snapped windows' bounds. If the window is already snapped in the
  // correct position, simply update the snap ratio.
  if (IsSnapped(primary_window_)) {
    UpdateSnappedBounds(primary_window_);
  }
  if (IsSnapped(secondary_window_)) {
    UpdateSnappedBounds(secondary_window_);
  }

  // Update divider's bounds and make it adjustable.
  if (split_view_divider_) {
    split_view_divider_->UpdateDividerBounds();

    // Make the split view divider adjustable.
    if (features::IsAdjustSplitViewForVKEnabled()) {
      split_view_divider_->SetAdjustable(true);
    }
  }
}

SplitViewController::SnapPosition SplitViewController::GetBlackScrimPosition(
    const gfx::Point& location_in_screen) {
  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  if (!work_area_bounds.Contains(location_in_screen))
    return SnapPosition::kNone;

  gfx::Size primary_window_min_size, secondary_window_min_size;
  if (primary_window_ && primary_window_->delegate())
    primary_window_min_size = primary_window_->delegate()->GetMinimumSize();
  if (secondary_window_ && secondary_window_->delegate())
    secondary_window_min_size = secondary_window_->delegate()->GetMinimumSize();

  bool right_side_up = IsLayoutPrimary(root_window_);
  int divider_end_position = GetDividerEndPosition();
  // The distance from the current resizing position to the left or right side
  // of the screen. Note: left or right side here means the side of the
  // |primary_window_| or |secondary_window_|.
  int primary_window_distance = 0, secondary_window_distance = 0;
  int min_left_length = 0, min_right_length = 0;

  if (IsLayoutHorizontal(root_window_)) {
    int left_distance = location_in_screen.x() - work_area_bounds.x();
    int right_distance = work_area_bounds.right() - location_in_screen.x();
    primary_window_distance = right_side_up ? left_distance : right_distance;
    secondary_window_distance = right_side_up ? right_distance : left_distance;

    min_left_length = primary_window_min_size.width();
    min_right_length = secondary_window_min_size.width();
  } else {
    int top_distance = location_in_screen.y() - work_area_bounds.y();
    int bottom_distance = work_area_bounds.bottom() - location_in_screen.y();
    primary_window_distance = right_side_up ? top_distance : bottom_distance;
    secondary_window_distance = right_side_up ? bottom_distance : top_distance;

    min_left_length = primary_window_min_size.height();
    min_right_length = secondary_window_min_size.height();
  }

  if (primary_window_distance < divider_end_position * kOneThirdSnapRatio ||
      primary_window_distance < min_left_length) {
    return SnapPosition::kPrimary;
  }
  if (secondary_window_distance < divider_end_position * kOneThirdSnapRatio ||
      secondary_window_distance < min_right_length) {
    return SnapPosition::kSecondary;
  }

  return SnapPosition::kNone;
}

void SplitViewController::UpdateDividerPosition(
    const gfx::Point& location_in_screen) {
  if (IsLayoutHorizontal(root_window_))
    divider_position_ += location_in_screen.x() - previous_event_location_.x();
  else
    divider_position_ += location_in_screen.y() - previous_event_location_.y();
  divider_position_ = std::max(0, divider_position_);
}

int SplitViewController::GetClosestFixedDividerPosition() {
  // The values in |kFixedPositionRatios| represent the fixed position of the
  // center of the divider while |divider_position_| represent the origin of the
  // divider rectangle. So, before calling FindClosestFixedPositionRatio,
  // extract the center from |divider_position_|. The result will also be the
  // center of the divider, so extract the origin, unless the result is on of
  // the endpoints.
  int divider_end_position = GetDividerEndPosition();
  divider_closest_ratio_ = FindClosestPositionRatio(
      divider_position_ + kSplitviewDividerShortSideLength / 2,
      divider_end_position);
  int fix_position = divider_end_position * divider_closest_ratio_;
  if (divider_closest_ratio_ > 0.f && divider_closest_ratio_ < 1.f)
    fix_position -= kSplitviewDividerShortSideLength / 2;
  return fix_position;
}

void SplitViewController::StopAndShoveAnimatedDivider() {
  DCHECK(IsDividerAnimating());

  divider_snap_animation_->Stop();
  divider_position_ = divider_snap_animation_->ending_position();
  NotifyDividerPositionChanged();
  UpdateSnappedWindowsAndDividerBounds();
}

bool SplitViewController::ShouldEndTabletSplitViewAfterResizing() {
  DCHECK(InTabletSplitViewMode());

  return divider_position_ == 0 || divider_position_ == GetDividerEndPosition();
}

void SplitViewController::EndTabletSplitViewAfterResizingIfAppropriate() {
  if (!ShouldEndTabletSplitViewAfterResizing())
    return;

  aura::Window* active_window = GetActiveWindowAfterResizingUponExit();

  // Track the window that needs to be put back into the overview list if we
  // remain in overview mode.
  aura::Window* insert_overview_window = nullptr;
  if (Shell::Get()->overview_controller()->InOverviewSession())
    insert_overview_window = GetDefaultSnappedWindow();
  EndSplitView();
  if (active_window) {
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEndAction::kSplitView);
    wm::ActivateWindow(active_window);
  } else if (insert_overview_window) {
    InsertWindowToOverview(insert_overview_window, /*animate=*/false);
  }
}

aura::Window* SplitViewController::GetActiveWindowAfterResizingUponExit() {
  DCHECK(InSplitViewMode());

  if (!ShouldEndTabletSplitViewAfterResizing())
    return nullptr;

  return divider_position_ == 0 ? GetPhysicalRightOrBottomWindow()
                                : GetPhysicalLeftOrTopWindow();
}

int SplitViewController::GetDividerEndPosition() const {
  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  return IsLayoutHorizontal(root_window_) ? work_area_bounds.width()
                                          : work_area_bounds.height();
}

void SplitViewController::OnWindowSnapped(
    aura::Window* window,
    absl::optional<chromeos::WindowStateType> previous_state) {
  RestoreTransformIfApplicable(window);
  UpdateStateAndNotifyObservers();
  UpdateWindowStackingAfterSnap(window);

  // If the snapped window was removed from overview and was the active window
  // before entering overview, it should be the active window after snapping in
  // splitview.
  if (to_be_activated_window_ == window) {
    to_be_activated_window_ = nullptr;
    wm::ActivateWindow(window);
  }

  // In tablet mode, if the window was previously floated, and there is another
  // non-minimized window, do not enter overview but instead snap that window to
  // the opposite side.
  if (previous_state &&
      *previous_state == chromeos::WindowStateType::kFloated &&
      Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    auto mru_windows =
        Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(
            kActiveDesk);
    for (aura::Window* mru_window : mru_windows) {
      auto* window_state = WindowState::Get(mru_window);
      if (mru_window != window && !window_state->IsMinimized() &&
          window_state->CanSnap()) {
        const SnapPosition snap_position =
            GetPositionOfSnappedWindow(window) == SnapPosition::kPrimary
                ? SnapPosition::kSecondary
                : SnapPosition::kPrimary;
        WMEvent event(snap_position == SnapPosition::kPrimary
                          ? WM_EVENT_SNAP_PRIMARY
                          : WM_EVENT_SNAP_SECONDARY);
        WindowState::Get(mru_window)->OnWMEvent(&event);
        return;
      }
    }
  }

  // If in tablet split view, make sure overview is opened on the other side of
  // the split if there is only one snapped window in split screen.
  auto* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession() &&
      split_view_type_ == SplitViewType::kTabletType &&
      (state_ == State::kPrimarySnapped ||
       state_ == State::kSecondarySnapped)) {
    overview_controller->StartOverview(OverviewStartAction::kSplitView,
                                       OverviewEnterExitType::kNormal);
  }
}

void SplitViewController::OnSnappedWindowDetached(aura::Window* window,
                                                  WindowDetachedReason reason) {
  const bool is_window_destroyed =
      reason == WindowDetachedReason::kWindowDestroyed;
  const SnapPosition position_of_snapped_window =
      GetPositionOfSnappedWindow(window);

  // Detach it from splitview first if the window is to be destroyed to prevent
  // unnecessary bounds/state update to it when ending splitview resizing. For
  // the window that is not going to be destroyed, we still need its bounds and
  // state to be updated to match the updated divider position before detaching
  // it from splitview.
  if (is_window_destroyed)
    StopObserving(position_of_snapped_window);

  // Stop resizing if one of the snapped window is detached from split
  // view.
  const bool is_divider_animating = IsDividerAnimating();
  if (is_resizing_ || is_divider_animating) {
    is_resizing_ = false;
    if (is_divider_animating)
      StopAndShoveAnimatedDivider();
    EndResizeImpl();
  }

  if (!is_window_destroyed)
    StopObserving(position_of_snapped_window);

  if (!primary_window_ && !secondary_window_) {
    // If there is no snapped window at this moment, ends split view mode. Note
    // this will update overview window grid bounds if the overview mode is
    // active at the moment.
    EndSplitView(reason == WindowDetachedReason::kWindowDragged
                     ? EndReason::kWindowDragStarted
                     : EndReason::kNormal);

    // TODO(crbug.com/1351562): Consider not allowing one snapped window to be
    // floated. Then this should be a DCHECK.
  } else {
    DCHECK_EQ(split_view_type_, SplitViewType::kTabletType);

    if (reason == WindowDetachedReason::kWindowFloated &&
        Shell::Get()->tablet_mode_controller()->InTabletMode()) {
      // Maximize the other window, which will end split view.
      aura::Window* other_window =
          GetSnappedWindow(position_of_snapped_window == SnapPosition::kPrimary
                               ? SnapPosition::kSecondary
                               : SnapPosition::kPrimary);
      WMEvent event(WM_EVENT_MAXIMIZE);
      WindowState::Get(other_window)->OnWMEvent(&event);
      return;
    }

    // If there is still one snapped window after minimizing/closing one snapped
    // window, update its snap state and open overview window grid.
    default_snap_position_ =
        primary_window_ ? SnapPosition::kPrimary : SnapPosition::kSecondary;
    UpdateStateAndNotifyObservers();
    Shell::Get()->overview_controller()->StartOverview(
        OverviewStartAction::kSplitView,
        reason == WindowDetachedReason::kWindowDragged
            ? OverviewEnterExitType::kImmediateEnter
            : OverviewEnterExitType::kNormal);
  }
}

float SplitViewController::FindClosestPositionRatio(float distance,
                                                    float length) {
  float current_ratio = distance / length;
  float closest_ratio = 0.f;
  std::vector<float> position_ratios(
      kFixedPositionRatios,
      kFixedPositionRatios + sizeof(kFixedPositionRatios) / sizeof(float));
  GetDividerOptionalPositionRatios(&position_ratios);
  for (float ratio : position_ratios) {
    if (std::abs(current_ratio - ratio) <
        std::abs(current_ratio - closest_ratio)) {
      closest_ratio = ratio;
    }
  }
  return closest_ratio;
}

void SplitViewController::GetDividerOptionalPositionRatios(
    std::vector<float>* out_position_ratios) {
  const bool landscape = IsCurrentScreenOrientationLandscape();
  const int min_left_size =
      GetMinimumWindowLength(GetPhysicalLeftOrTopWindow(), landscape);
  const int min_right_size =
      GetMinimumWindowLength(GetPhysicalRightOrBottomWindow(), landscape);
  const int divider_end_position = GetDividerEndPosition();
  const float min_size_left_ratio =
      static_cast<float>(min_left_size) / divider_end_position;
  const float min_size_right_ratio =
      static_cast<float>(min_right_size) / divider_end_position;
  if (min_size_left_ratio <= kOneThirdSnapRatio)
    out_position_ratios->push_back(kOneThirdSnapRatio);
  if (min_size_right_ratio <= kOneThirdSnapRatio)
    out_position_ratios->push_back(kTwoThirdSnapRatio);
}

int SplitViewController::GetWindowComponentForResize(aura::Window* window) {
  DCHECK(IsWindowInSplitView(window));
  return window == primary_window_ ? HTRIGHT : HTLEFT;
}

gfx::Point SplitViewController::GetEndDragLocationInScreen(
    aura::Window* window,
    const gfx::Point& location_in_screen) {
  gfx::Point end_location(location_in_screen);
  if (!IsWindowInSplitView(window))
    return end_location;

  const gfx::Rect bounds = GetSnappedWindowBoundsInScreen(
      GetPositionOfSnappedWindow(window), window);
  if (IsLayoutHorizontal(window)) {
    end_location.set_x(window == GetPhysicalLeftOrTopWindow() ? bounds.right()
                                                              : bounds.x());
  } else {
    end_location.set_y(window == GetPhysicalLeftOrTopWindow() ? bounds.bottom()
                                                              : bounds.y());
  }
  return end_location;
}

void SplitViewController::RestoreTransformIfApplicable(aura::Window* window) {
  // If the transform of the window has been changed, calculate a good starting
  // transform based on its transformed bounds before to be snapped.
  auto iter = snapping_window_transformed_bounds_map_.find(window);
  if (iter == snapping_window_transformed_bounds_map_.end())
    return;

  const gfx::Rect item_bounds = iter->second;
  snapping_window_transformed_bounds_map_.erase(iter);

  // Restore the window's transform first if it's not identity.
  if (!window->layer()->GetTargetTransform().IsIdentity()) {
    // Calculate the starting transform based on the window's expected snapped
    // bounds and its transformed bounds before to be snapped.
    const gfx::Rect snapped_bounds = GetSnappedWindowBoundsInScreen(
        GetPositionOfSnappedWindow(window), window);
    const gfx::Transform starting_transform = gfx::TransformBetweenRects(
        gfx::RectF(snapped_bounds), gfx::RectF(item_bounds));
    SetTransformWithAnimation(window, starting_transform, gfx::Transform());
  }
}

void SplitViewController::UpdateWindowStackingAfterSnap(
    aura::Window* newly_snapped) {
  if (split_view_divider_)
    split_view_divider_->SetAlwaysOnTop(true);

  aura::Window* other_snapped =
      newly_snapped == primary_window_ ? secondary_window_ : primary_window_;
  if (other_snapped) {
    DCHECK(newly_snapped == primary_window_ ||
           newly_snapped == secondary_window_);
    other_snapped->parent()->StackChildAtTop(other_snapped);
  }

  newly_snapped->parent()->StackChildAtTop(newly_snapped);
}

void SplitViewController::SetWindowsTransformDuringResizing() {
  DCHECK(InTabletSplitViewMode());
  DCHECK_GE(divider_position_, 0);
  const bool horizontal = IsLayoutHorizontal(root_window_);
  aura::Window* left_or_top_window = GetPhysicalLeftOrTopWindow();
  aura::Window* right_or_bottom_window = GetPhysicalRightOrBottomWindow();

  gfx::Transform left_or_top_transform;
  if (left_or_top_window) {
    const int left_size = divider_position_;
    const int distance =
        left_size - GetWindowLength(left_or_top_window, horizontal);
    if (distance < 0) {
      left_or_top_transform.Translate(horizontal ? distance : 0,
                                      horizontal ? 0 : distance);
    }
    SetTransform(left_or_top_window, left_or_top_transform);
  }

  gfx::Transform right_or_bottom_transform;
  if (right_or_bottom_window) {
    const int right_size = GetDividerEndPosition() - divider_position_ -
                           kSplitviewDividerShortSideLength;
    const int distance =
        right_size - GetWindowLength(right_or_bottom_window, horizontal);
    if (distance < 0) {
      right_or_bottom_transform.Translate(horizontal ? -distance : 0,
                                          horizontal ? 0 : -distance);
    }
    SetTransform(right_or_bottom_window, right_or_bottom_transform);
  }
}

void SplitViewController::RestoreWindowsTransformAfterResizing() {
  DCHECK(InSplitViewMode());
  if (primary_window_)
    SetTransform(primary_window_, gfx::Transform());
  if (secondary_window_)
    SetTransform(secondary_window_, gfx::Transform());
  if (black_scrim_layer_.get())
    black_scrim_layer_->SetTransform(gfx::Transform());
}

void SplitViewController::SetTransformWithAnimation(
    aura::Window* window,
    const gfx::Transform& start_transform,
    const gfx::Transform& target_transform) {
  const gfx::PointF target_origin = GetTargetBoundsInScreen(window).origin();
  for (auto* window_iter : GetTransientTreeIterator(window)) {
    // Adjust |start_transform| and |target_transform| for the transient child.
    aura::Window* parent_window = window_iter->parent();
    gfx::RectF original_bounds(window_iter->GetTargetBounds());
    ::wm::TranslateRectToScreen(parent_window, &original_bounds);
    const gfx::PointF pivot(target_origin.x() - original_bounds.x(),
                            target_origin.y() - original_bounds.y());
    const gfx::Transform new_start_transform =
        TransformAboutPivot(pivot, start_transform);
    const gfx::Transform new_target_transform =
        TransformAboutPivot(pivot, target_transform);
    if (new_start_transform != window_iter->layer()->GetTargetTransform())
      window_iter->SetTransform(new_start_transform);

    std::vector<ui::ImplicitAnimationObserver*> animation_observers;
    if (window_iter == window) {
      animation_observers.push_back(
          new WindowTransformAnimationObserver(window));

      // If the overview exit animation is in progress or is about to start, add
      // the |window| snap animation as one of the animations to be completed
      // before |OverviewController::OnEndingAnimationComplete| should be called
      // to unpause occlusion tracking, unblur the wallpaper, etc.
      OverviewController* overview_controller =
          Shell::Get()->overview_controller();
      if (overview_controller->IsCompletingShutdownAnimations() ||
          (overview_controller->overview_session() &&
           overview_controller->overview_session()->is_shutting_down() &&
           overview_controller->overview_session()
                   ->enter_exit_overview_type() !=
               OverviewEnterExitType::kImmediateExit)) {
        auto overview_exit_animation_observer =
            std::make_unique<ExitAnimationObserver>();
        animation_observers.push_back(overview_exit_animation_observer.get());
        overview_controller->AddExitAnimationObserver(
            std::move(overview_exit_animation_observer));
      }
    }
    DoSplitviewTransformAnimation(window_iter->layer(),
                                  SPLITVIEW_ANIMATION_SET_WINDOW_TRANSFORM,
                                  new_target_transform, animation_observers);
  }
}

void SplitViewController::UpdateSnappingWindowTransformedBounds(
    aura::Window* window) {
  if (!window->layer()->GetTargetTransform().IsIdentity()) {
    snapping_window_transformed_bounds_map_[window] = gfx::ToEnclosedRect(
        window_util::GetTransformedBounds(window, /*top_inset=*/0));
  }
}

void SplitViewController::InsertWindowToOverview(aura::Window* window,
                                                 bool animate) {
  if (!window || !GetOverviewSession())
    return;
  GetOverviewSession()->AddItemInMruOrder(window, /*reposition=*/true, animate,
                                          /*restack=*/true,
                                          /*use_spawn_animation=*/false);
}

void SplitViewController::FinishWindowResizing(aura::Window* window) {
  if (window != nullptr) {
    WindowState* window_state = WindowState::Get(window);
    window_state->OnCompleteDrag(gfx::PointF(
        GetEndDragLocationInScreen(window, previous_event_location_)));
    window_state->DeleteDragDetails();
  }
}

void SplitViewController::EndResizeImpl() {
  DCHECK(InSplitViewMode());
  DCHECK(!is_resizing_);

  // The backdrop layers are removed here (rather than in `EndResize()`) since
  // they may be used while the divider is animating to a snapped position.
  left_resize_backdrop_layer_.reset();
  right_resize_backdrop_layer_.reset();

  // Resize may not end with |EndResize()|, so make sure to clear here too.
  resize_timer_.Stop();
  presentation_time_recorder_.reset();
  RestoreWindowsTransformAfterResizing();
  FinishWindowResizing(primary_window_);
  FinishWindowResizing(secondary_window_);
}

void SplitViewController::OnResizeTimer() {
  if (InSplitViewMode())
    Resize(previous_event_location_);
}

void SplitViewController::UpdateTabletResizeMode(
    base::TimeTicks event_time_ticks,
    const gfx::Point& event_location) {
  if (IsLayoutHorizontal(root_window_)) {
    accumulated_drag_distance_ +=
        std::abs(event_location.x() - previous_event_location_.x());
  } else {
    accumulated_drag_distance_ +=
        std::abs(event_location.y() - previous_event_location_.y());
  }

  const base::TimeDelta chunk_time_ticks =
      event_time_ticks - accumulated_drag_time_ticks_;
  // We switch between fast and normal resize mode depending on how fast the
  // divider is dragged. This is done in "chunks" by keeping track of how far
  // the divider has been dragged. When the chunk gone on for long enough, we
  // calculate the drag speed based on `accumulated_drag_distance_` and update
  // the resize mode accordingly.
  if (chunk_time_ticks >= kSplitViewChunkTime) {
    int drag_per_second =
        accumulated_drag_distance_ / chunk_time_ticks.InSecondsF();
    tablet_resize_mode_ = drag_per_second > kSplitViewThresholdPixelsPerSec
                              ? TabletResizeMode::kFast
                              : TabletResizeMode::kNormal;

    accumulated_drag_time_ticks_ = event_time_ticks;
    accumulated_drag_distance_ = 0;
  }
}

void SplitViewController::EndWindowDragImpl(
    aura::Window* window,
    bool is_being_destroyed,
    SnapPosition desired_snap_position,
    const gfx::Point& last_location_in_screen) {
  if (split_view_divider_)
    split_view_divider_->OnWindowDragEnded();

  // If the dragged window is to be destroyed, do not try to snap it.
  if (is_being_destroyed)
    return;

  // If dragged window was in overview before or it has been added to overview
  // window by dropping on the new selector item, do nothing.
  if (GetOverviewSession() && GetOverviewSession()->IsWindowInOverview(window))
    return;

  DCHECK_EQ(root_window_, window->GetRootWindow());

  const bool was_splitview_active = InSplitViewMode();
  if (desired_snap_position == SnapPosition::kNone) {
    if (was_splitview_active) {
      // Even though |snap_position| equals |SnapPosition::kNone|, the dragged
      // window still needs to be snapped if splitview mode is active at the
      // moment.
      // Calculate the expected snap position based on the last event
      // location. Note if there is already a window at |desired_snap_postion|,
      // SnapWindow() will put the previous snapped window in overview.
      SnapWindow(window, ComputeSnapPosition(last_location_in_screen),
                 /*activate_window=*/true);
    } else {
      // Restore the dragged window's transform first if it's not identity. It
      // needs to be called before the transformed window's bounds change so
      // that its transient children are layout'ed properly (the layout happens
      // when window's bounds change).
      SetTransformWithAnimation(window, window->layer()->GetTargetTransform(),
                                gfx::Transform());

      OverviewSession* overview_session = GetOverviewSession();
      if (overview_session) {
        overview_session->SetWindowListNotAnimatedWhenExiting(root_window_);
        // Set the overview exit type to kImmediateExit to avoid update bounds
        // animation of the windows in overview grid.
        overview_session->set_enter_exit_overview_type(
            OverviewEnterExitType::kImmediateExit);
      }
      // Activate the dragged window and end the overview. The dragged window
      // will be restored back to its previous state before dragging.
      wm::ActivateWindow(window);
      Shell::Get()->overview_controller()->EndOverview(
          OverviewEndAction::kSplitView);

      // Update the dragged window's bounds. It's possible that the dragged
      // window's bounds was changed during dragging. Update its bounds after
      // the drag ends to ensure it has the right bounds.
      TabletModeWindowState::UpdateWindowPosition(
          WindowState::Get(window),
          WindowState::BoundsChangeAnimationType::kAnimate);
    }
  } else {
    aura::Window* initiator_window =
        window->GetProperty(kTabDraggingSourceWindowKey);
    // Note SnapWindow() might put the previous window that was snapped at the
    // |desired_snap_position| in overview.
    SnapWindow(window, desired_snap_position, /*activate_window=*/true);

    if (!was_splitview_active && initiator_window &&
        initiator_window != window) {
      // If splitview mode was not active before snapping the dragged
      // window, snap the initiator window to the other side of the screen
      // if it's not the same window as the dragged window.
      SnapWindow(initiator_window,
                 (desired_snap_position == SnapPosition::kPrimary)
                     ? SnapPosition::kSecondary
                     : SnapPosition::kPrimary);
    }
  }
}

void SplitViewController::DoSplitDividerSpawnAnimation(aura::Window* window) {
  DCHECK(window->layer()->GetAnimator()->GetTargetTransform().IsIdentity());
  SnapPosition snap_position = GetPositionOfSnappedWindow(window);
  const gfx::Rect bounds =
      GetSnappedWindowBoundsInScreen(snap_position, window);
  // Get one of the two corners of |window| that meet the divider.
  gfx::Point p = IsPhysicalLeftOrTop(snap_position, window)
                     ? bounds.bottom_right()
                     : bounds.origin();
  // Apply the transform that |window| will undergo when the divider spawns.
  static const double value = gfx::Tween::CalculateValue(
      gfx::Tween::FAST_OUT_SLOW_IN,
      kSplitviewDividerSpawnDelay / kSplitviewWindowTransformDuration);
  p = gfx::TransformAboutPivot(
          gfx::PointF(bounds.origin()),
          gfx::Tween::TransformValueBetween(value, window->transform(),
                                            gfx::Transform()))
          .MapPoint(p);
  // Use a coordinate of the transformed |window| corner for spawn_position.
  split_view_divider_->DoSpawningAnimation(IsLayoutHorizontal(window) ? p.x()
                                                                      : p.y());
}

}  // namespace ash

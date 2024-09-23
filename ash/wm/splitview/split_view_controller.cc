// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/wm/splitview/split_view_controller.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/delayed_animation_observer_impl.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_metrics.h"
#include "ash/wm/splitview/auto_snap_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_metrics_controller.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "ash/wm/splitview/split_view_overview_session.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_metrics.h"
#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "components/app_restore/desk_template_read_handler.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/input_method.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/compositor_animation_runner.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

using chromeos::WindowStateType;

// Five fixed position ratios of the divider, which means the divider can
// always be moved to these five positions.
constexpr float kFixedPositionRatios[] = {0.f, chromeos::kOneThirdSnapRatio,
                                          chromeos::kDefaultSnapRatio,
                                          chromeos::kTwoThirdSnapRatio, 1.0f};

// The black scrim starts to fade in when the divider is moved past the two
// optional positions (`chromeos::kOneThirdSnapRatio`,
// `chromeos::kTwoThirdSnapRatio`) and reaches to its maximum opacity
// (`kBlackScrimOpacity`) after moving `kBlackScrimFadeInRatio` of the screen
// width. See https://crbug.com/827730 for details.
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
// following conditions:
// a) tablet split view, one snapped window, empty overview grid;
// b) tablet split view, two snapped windows;
// c) tablet split view, one snapped window, nonempty overview grid;
constexpr char kTabletSplitViewResizeSingleHistogram[] =
    "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow";
constexpr char kTabletSplitViewResizeMultiHistogram[] =
    "Ash.SplitViewResize.PresentationTime.TabletMode.MultiWindow";
constexpr char kTabletSplitViewResizeWithOverviewHistogram[] =
    "Ash.SplitViewResize.PresentationTime.TabletMode.WithOverview";

constexpr char kTabletSplitViewResizeSingleMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow";
constexpr char kTabletSplitViewResizeMultiMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.MultiWindow";
constexpr char kTabletSplitViewResizeWithOverviewMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.WithOverview";

// The time when the number of roots in split view changes from one to two. Used
// for the purpose of metric collection.
base::Time g_multi_display_split_view_start_time;

bool g_use_fast_resize_for_testing = false;

bool InTabletMode() {
  return display::Screen::GetScreen()->InTabletMode();
}

bool IsExactlyOneRootInSplitView() {
  const aura::Window::Windows all_root_windows = Shell::GetAllRootWindows();
  return 1 ==
         base::ranges::count_if(
             all_root_windows, [](aura::Window* root_window) {
               return SplitViewController::Get(root_window)->InSplitViewMode();
             });
}

ui::InputMethod* GetCurrentInputMethod() {
  if (auto* bridge = IMEBridge::Get()) {
    if (auto* handler = bridge->GetInputContextHandler())
      return handler->GetInputMethod();
  }
  return nullptr;
}

void RemoveSnappingWindowFromOverviewIfApplicable(
    OverviewSession* overview_session,
    aura::Window* window) {
  if (!overview_session) {
    return;
  }

  OverviewItemBase* item = overview_session->GetOverviewItemForWindow(window);
  if (!item) {
    return;
  }

  // Remove it from overview. The transform will be reset later after the window
  // is snapped. Note the remaining windows in overview don't need to be
  // repositioned in this case as they have been positioned to the right place
  // during dragging.
  item->EnsureVisible();
  item->RestoreWindow(/*reset_transform=*/false, /*animate=*/true);
  overview_session->RemoveItem(item);
}

// If there is a window in the snap position, trigger a WMEvent to snap it in
// the corresponding position.
void TriggerWMEventToSnapWindow(WindowState* window_state,
                                WMEventType event_type) {
  CHECK(event_type == WM_EVENT_SNAP_PRIMARY ||
        event_type == WM_EVENT_SNAP_SECONDARY);

  const WindowSnapWMEvent window_event(
      event_type,
      window_state->snap_ratio().value_or(chromeos::kDefaultSnapRatio));
  window_state->OnWMEvent(&window_event);
}

// Returns true if the snap state of the `window` has changed if it's already in
// split view mode.
bool DidInSplitViewWindowChange(aura::Window* window,
                                SplitViewController* split_view_controller,
                                SnapPosition snap_position) {
  if (!split_view_controller->IsWindowInSplitView(window)) {
    return false;
  }

  const auto* window_state = WindowState::Get(window);
  if (window_state->GetStateType() !=
      GetWindowStateTypeFromSnapPosition(snap_position)) {
    return true;
  }

  // For the current tablet mode split view design, we can assume that the
  // `window` is being snapped to the same `snap_position` it was snapped since
  // it's single layer design. We need to check if the snap ratio is the same.
  std::optional<float> snap_ratio = window_state->snap_ratio();
  // Get the snap ratio for the window that is currently occupying the
  // `snap_position`.
  const auto* window_state_in_current_snap_position =
      WindowState::Get(split_view_controller->GetSnappedWindow(snap_position));
  const bool same_snap_ratio =
      snap_ratio && window_state_in_current_snap_position &&
      *snap_ratio == window_state_in_current_snap_position->snap_ratio();
  return !same_snap_ratio;
}

}  // namespace

// -----------------------------------------------------------------------------
// DividerSnapAnimation:

// Animates the divider to its closest fixed position.
// `SplitViewController::IsResizingWithDivider()` is assumed to be already false
// before this animation starts, but some resizing logic is delayed until this
// animation ends.
class SplitViewController::DividerSnapAnimation
    : public gfx::SlideAnimation,
      public gfx::AnimationDelegate {
 public:
  DividerSnapAnimation(SplitViewController* split_view_controller,
                       int starting_position,
                       int ending_position,
                       base::TimeDelta duration,
                       gfx::Tween::Type tween_type)
      : gfx::SlideAnimation(this),
        split_view_controller_(split_view_controller),
        starting_position_(starting_position),
        ending_position_(ending_position) {
    SetSlideDuration(duration);
    SetTweenType(tween_type);

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
        metrics_util::ForSmoothnessV3(base::BindRepeating([](int smoothness) {
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
    DCHECK(!split_view_controller_->IsResizingWithDivider());
    DCHECK_EQ(ending_position_, split_view_controller_->GetDividerPosition());

    split_view_controller_->EndResizeWithDividerImpl();
    split_view_controller_->EndSplitViewAfterResizingAtEdgeIfAppropriate();

    if (tracker_)
      tracker_->Stop();
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    DCHECK(split_view_controller_->InSplitViewMode());
    DCHECK(!split_view_controller_->IsResizingWithDivider());

    // TODO(b/327685487): Remove these when the crash is fixed.
    const int divider_position_before_tween =
        split_view_controller_->GetDividerPosition();
    split_view_controller_->split_view_divider()->SetDividerPosition(
        CurrentValueBetween(starting_position_, ending_position_));
    const int divider_position_after_tween =
        split_view_controller_->GetDividerPosition();
    split_view_controller_->NotifyDividerPositionChanged();
    const int divider_position_after_notify =
        split_view_controller_->GetDividerPosition();
    split_view_controller_->UpdateSnappedWindowsAndDividerBounds();
    const int divider_position_after_update =
        split_view_controller_->GetDividerPosition();

    // Updating the window may stop animation.
    if (is_animating()) {
      // Map the tablet resize mode to a string.
      base::flat_map<SplitViewController::TabletResizeMode, std::string>
          resize_mode_as_string = {
              {SplitViewController::TabletResizeMode::kNormal, "kNormal"},
              {SplitViewController::TabletResizeMode::kFast, "kFast"},
          };
      SCOPED_CRASH_KEY_STRING32(
          "b327685487", "tablet_resize_mode",
          resize_mode_as_string[split_view_controller_->tablet_resize_mode_]);
      SCOPED_CRASH_KEY_BOOL("b327685487", "in_tablet_mode", InTabletMode());
      SCOPED_CRASH_KEY_BOOL(
          "b327685487", "has_divider_widget",
          !!split_view_controller_->split_view_divider()->divider_widget());
      SCOPED_CRASH_KEY_BOOL("b327685487", "in_split_view",
                            split_view_controller_->InSplitViewMode());
      SCOPED_CRASH_KEY_BOOL("b327685487", "is_divider_resizing",
                            split_view_controller_->IsResizingWithDivider());
      SCOPED_CRASH_KEY_NUMBER("b327685487", "before_tween",
                              divider_position_before_tween);
      SCOPED_CRASH_KEY_NUMBER("b327685487", "after_tween",
                              divider_position_after_tween);
      SCOPED_CRASH_KEY_NUMBER("b327685487", "after_notify",
                              divider_position_after_notify);
      SCOPED_CRASH_KEY_NUMBER("b327685487", "after_update",
                              divider_position_after_update);
      SCOPED_CRASH_KEY_NUMBER("b327685487", "starting_position",
                              starting_position_);
      SCOPED_CRASH_KEY_NUMBER("b327685487", "ending_position",
                              ending_position_);
      split_view_controller_->UpdateResizeBackdrop();
      split_view_controller_->SetWindowsTransformDuringResizing();
    }
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    if (tracker_)
      tracker_->Cancel();
  }

  raw_ptr<SplitViewController> split_view_controller_;
  int starting_position_;
  int ending_position_;
  std::optional<ui::ThroughputTracker> tracker_;
};

// -----------------------------------------------------------------------------
// ToBeSnappedWindowsObserver:

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
    for (auto& to_be_snapped_window : to_be_snapped_windows_) {
      if (aura::Window* window = to_be_snapped_window.second.window) {
        window->RemoveObserver(this);
        WindowState::Get(window)->RemoveObserver(this);
      }
    }
    to_be_snapped_windows_.clear();
  }

  void AddToBeSnappedWindow(aura::Window* window,
                            SnapPosition snap_position,
                            WindowSnapActionSource snap_action_source) {
    if (DidInSplitViewWindowChange(window, split_view_controller_,
                                   snap_position)) {
      split_view_controller_->AttachToBeSnappedWindow(window, snap_position,
                                                      snap_action_source);
      return;
    }

    aura::Window* old_window = to_be_snapped_windows_[snap_position].window;
    if (old_window == window) {
      return;
    }

    // Stop observing any previous to-be-snapped window in `snap_position`. This
    // can happen to Android windows as its window state and bounds change are
    // async, so it's possible to snap another window to the same position while
    // waiting for the snapping of the previous window.
    if (old_window) {
      to_be_snapped_windows_.erase(snap_position);
      WindowState::Get(old_window)->RemoveObserver(this);
      old_window->RemoveObserver(this);
    }

    // If the to-be-snapped window already has the desired snapped window state,
    // no need to listen to the state change notification (there will be none
    // anyway), instead just attach the window to split screen directly.
    WindowState* window_state = WindowState::Get(window);
    if (window_state->GetStateType() ==
        GetWindowStateTypeFromSnapPosition(snap_position)) {
      split_view_controller_->AttachToBeSnappedWindow(window, snap_position,
                                                      snap_action_source);
      split_view_controller_->OnWindowSnapped(window,
                                              /*previous_state=*/std::nullopt,
                                              snap_action_source);
    } else {
      to_be_snapped_windows_[snap_position] =
          WindowAndSnapSourceInfo{window, snap_action_source};
      window_state->AddObserver(this);
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
    aura::Window* window = window_state->window();
    // When arriving here, we know the to-be-snapped window's state has just
    // changed and its bounds will be changed soon.
    auto iter = FindWindow(window);
    DCHECK(iter != to_be_snapped_windows_.end());
    SnapPosition snap_position = iter->first;

    // If the new window type is the target snapped state, remove the window
    // from `to_be_snapped_windows_` and do some prep work for snapping it in
    // split screen. Otherwise (i.e. if the new window type is not the target
    // one) just ignore the event and keep waiting for the next event.
    if (window_state->GetStateType() ==
        GetWindowStateTypeFromSnapPosition(snap_position)) {
      const auto cached_snap_action_source = iter->second.snap_action_source;
      to_be_snapped_windows_.erase(iter);
      window_state->RemoveObserver(this);
      window->RemoveObserver(this);
      split_view_controller_->AttachToBeSnappedWindow(
          window, snap_position, cached_snap_action_source);
    }
  }

 private:
  // Contains the info of the window to be snapped and its corresponding snap
  // action source.
  struct WindowAndSnapSourceInfo {
    raw_ptr<aura::Window> window = nullptr;
    WindowSnapActionSource snap_action_source =
        WindowSnapActionSource::kNotSpecified;
  };

  base::flat_map<SnapPosition, WindowAndSnapSourceInfo>::const_iterator
  FindWindow(const aura::Window* window) const {
    for (auto iter = to_be_snapped_windows_.begin();
         iter != to_be_snapped_windows_.end(); iter++) {
      if (iter->second.window == window) {
        return iter;
      }
    }
    return to_be_snapped_windows_.end();
  }

  const raw_ptr<SplitViewController> split_view_controller_;

  // Maps the snap position to the to-be-snapped window with its corresponding
  // snap action source.
  base::flat_map<SnapPosition, WindowAndSnapSourceInfo> to_be_snapped_windows_;
};

// static
SplitViewController* SplitViewController::Get(const aura::Window* window) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());
  DCHECK(RootWindowController::ForWindow(window));
  return RootWindowController::ForWindow(window)->split_view_controller();
}

// -----------------------------------------------------------------------------
// SplitViewController:

SplitViewController::SplitViewController(aura::Window* root_window)
    : root_window_(root_window),
      to_be_snapped_windows_observer_(
          std::make_unique<ToBeSnappedWindowsObserver>(this)),
      split_view_divider_(this),
      split_view_metrics_controller_(
          std::make_unique<SplitViewMetricsController>(this)) {
  Shell::Get()->accessibility_controller()->AddObserver(this);
}

SplitViewController::~SplitViewController() {
  if (AccessibilityController* a11y_controller =
          Shell::Get()->accessibility_controller()) {
    a11y_controller->RemoveObserver(this);
  }

  EndSplitView(EndReason::kRootWindowDestroyed);
}

int SplitViewController::GetDividerPosition() const {
  return split_view_divider_.divider_position();
}

bool SplitViewController::IsResizingWithDivider() const {
  return split_view_divider_.HasDividerWidget() &&
         split_view_divider_.is_resizing_with_divider();
}

bool SplitViewController::InSplitViewMode() const {
  return state_ != State::kNoSnap;
}

bool SplitViewController::InClamshellSplitViewMode() const {
  return InSplitViewMode() && !InTabletMode();
}

bool SplitViewController::InTabletSplitViewMode() const {
  return InSplitViewMode() && InTabletMode();
}

bool SplitViewController::CanSnapWindow(aura::Window* window,
                                        float snap_ratio) const {
  if (!ShouldAllowSplitView())
    return false;

  if (!WindowState::Get(window)->CanSnapOnDisplay(
          display::Screen::GetScreen()->GetDisplayNearestWindow(
              const_cast<aura::Window*>(root_window_.get())))) {
    return false;
  }

  // Windows created by window restore are not activatable while being restored.
  // However, we still want to be able to snap these windows at this point.
  const bool is_to_be_restored_window =
      window == WindowRestoreController::Get()->to_be_snapped_window();

  // TODO(sammiequon): Investigate if we need to check for window activation.
  if (!is_to_be_restored_window && !wm::CanActivateWindow(window))
    return false;

  // We only need to consider the divider width in tablet mode or Snap Groups.
  const int divider_delta =
      ShouldConsiderDivider() ? kSplitviewDividerShortSideLength / 2 : 0;

  return GetMinimumWindowLength(window, IsLayoutHorizontal(window)) <=
         GetDividerPositionUpperLimit(root_window_) * snap_ratio -
             divider_delta;
}

bool SplitViewController::CanKeepCurrentSnapRatio(
    aura::Window* snapped_window) const {
  return CanSnapWindow(snapped_window,
                       WindowState::Get(snapped_window)
                           ->snap_ratio()
                           .value_or(chromeos::kDefaultSnapRatio));
}

std::optional<float> SplitViewController::ComputeAutoSnapRatio(
    aura::Window* window) {
  // If there is no default snapped window, or it doesn't have a stored snap
  // ratio try snapping it to 1/2.
  aura::Window* default_window = GetDefaultSnappedWindow();
  std::optional<float> default_window_snap_ratio =
      default_window ? WindowState::Get(default_window)->snap_ratio()
                     : std::nullopt;
  if (!default_window_snap_ratio) {
    return CanSnapWindow(window, chromeos::kDefaultSnapRatio)
               ? std::make_optional(chromeos::kDefaultSnapRatio)
               : std::nullopt;
  }

  // Maps the snap ratio of the default window to the snap ratio of the opposite
  // window.
  static constexpr auto kOppositeRatiosMap =
      base::MakeFixedFlatMap<float, float>(
          {{chromeos::kOneThirdSnapRatio, chromeos::kTwoThirdSnapRatio},
           {chromeos::kDefaultSnapRatio, chromeos::kDefaultSnapRatio},
           {chromeos::kTwoThirdSnapRatio, chromeos::kOneThirdSnapRatio}});
  auto it = kOppositeRatiosMap.find(*default_window_snap_ratio);
  // TODO(sammiequon): Investigate if this check is needed. It may be needed for
  // rounding errors (i.e. 2/3 may be 0.67).
  if (it == kOppositeRatiosMap.end()) {
    return CanSnapWindow(window, chromeos::kDefaultSnapRatio)
               ? std::make_optional(chromeos::kDefaultSnapRatio)
               : std::nullopt;
  }

  // If `window` can be snapped to the ideal snap ratio, we are done.
  float snap_ratio = it->second;
  if (CanSnapWindow(window, snap_ratio)) {
    return snap_ratio;
  }

  // Reaching here, we cannot snap `window` to its ideal snap ratio. If the
  // ideal snap ratio was 1/3, we try snapping to 1/2, but only if the default
  // window can be snapped to 1/2 as well.
  if (snap_ratio == chromeos::kOneThirdSnapRatio &&
      CanSnapWindow(window, chromeos::kDefaultSnapRatio) &&
      CanSnapWindow(default_window, chromeos::kDefaultSnapRatio)) {
    return chromeos::kDefaultSnapRatio;
  }

  return std::nullopt;
}

bool SplitViewController::WillStartPartialOverview(aura::Window* window) const {
  const bool can_start_in_tablet = InTabletMode() && !IsInOverviewSession();
  const bool can_start_in_clamshell =
      CanStartSplitViewOverviewSessionInClamshell(
          window, WindowState::Get(window)->snap_action_source().value_or(
                      WindowSnapActionSource::kNotSpecified));

  // Note that at this point `state_` may not have been updated yet, so check if
  // only one of `primary_window_` or `secondary_window_` is snapped.
  return (can_start_in_tablet || can_start_in_clamshell) &&
         !DesksController::Get()->animation() &&
         !!primary_window_ != !!secondary_window_;
}

void SplitViewController::SnapWindow(aura::Window* window,
                                     SnapPosition snap_position,
                                     WindowSnapActionSource snap_action_source,
                                     bool activate_window,
                                     float snap_ratio) {
  DCHECK(window && CanSnapWindow(window, snap_ratio));
  DCHECK_NE(snap_position, SnapPosition::kNone);
  if (IsDividerAnimating()) {
    StopSnapAnimation();
  }

  OverviewSession* overview_session = GetOverviewSession();
  if (activate_window ||
      (overview_session &&
       overview_session->IsWindowActiveWindowBeforeOverview(window))) {
    to_be_activated_window_ = window;
  }

  to_be_snapped_windows_observer_->AddToBeSnappedWindow(window, snap_position,
                                                        snap_action_source);
  // Move |window| to the display of |root_window_| first before sending the
  // WMEvent. Otherwise it may be snapped to the wrong display.
  if (root_window_ != window->GetRootWindow()) {
    window_util::MoveWindowToDisplay(window,
                                     display::Screen::GetScreen()
                                         ->GetDisplayNearestWindow(root_window_)
                                         .id());
  }
  const WindowSnapWMEvent event(snap_position == SnapPosition::kPrimary
                                    ? WM_EVENT_SNAP_PRIMARY
                                    : WM_EVENT_SNAP_SECONDARY,
                                snap_ratio, snap_action_source);
  WindowState::Get(window)->OnWMEvent(&event);

  base::RecordAction(base::UserMetricsAction("SplitView_SnapWindow"));
}

void SplitViewController::OnSnapEvent(
    aura::Window* window,
    WMEventType event_type,
    WindowSnapActionSource snap_action_source) {
  CHECK(event_type == WM_EVENT_SNAP_PRIMARY ||
        event_type == WM_EVENT_SNAP_SECONDARY);

  // If split view can't be enabled at the moment, do nothing.
  if (!ShouldAllowSplitView()) {
    return;
  }

  const bool in_overview = IsInOverviewSession();

  // In clamshell mode, only if overview is active on window snapped or in
  // faster split screen setup session, the window should be managed by
  // `SplitViewController`. Otherwise, the window should be managed by
  // `WindowState`.
  if (!InTabletMode() &&
      !(in_overview || ShouldConsiderWindowForSplitViewSetupView(
                           window, snap_action_source))) {
    return;
  }

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

  // Do nothing if `window` is already waiting to be snapped in split screen.
  // Order here matters: this must return for auto-snap windows before they try
  // to override `GetDividerPosition()` from a `new_snap_ratio` below.
  if (to_be_snapped_windows_observer_->IsObserving(window)) {
    return;
  }

  const SnapPosition to_snap_position = event_type == WM_EVENT_SNAP_PRIMARY
                                            ? SnapPosition::kPrimary
                                            : SnapPosition::kSecondary;
  // Start observing the to-be-snapped window.
  to_be_snapped_windows_observer_->AddToBeSnappedWindow(
      window, to_snap_position, snap_action_source);
}

void SplitViewController::AttachToBeSnappedWindow(
    aura::Window* window,
    SnapPosition snap_position,
    WindowSnapActionSource snap_action_source) {
  // Save the transformed bounds in preparation for the snapping animation.
  UpdateSnappingWindowTransformedBounds(window);

  OverviewSession* overview_session = GetOverviewSession();
  RemoveSnappingWindowFromOverviewIfApplicable(overview_session, window);

  if (state_ == State::kNoSnap) {
    default_snap_position_ = snap_position;

    // TODO(b/339066590): Add new histogram for splitview starting in
    // `OnWindowSnapped|UpdateStateAndNotifyObservers()`.
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
  aura::Window* other_window = nullptr;
  if (snap_position == SnapPosition::kPrimary) {
    if (primary_window_ != window) {
      previous_snapped_window = primary_window_;
      StopObserving(SnapPosition::kPrimary);
      primary_window_ = window;
    }
    if (secondary_window_ == window) {
      // Remove `window` from `secondary_window_` if it was previously snapped
      // there, i.e. during cycle snap or swap windows.
      secondary_window_ = nullptr;
      default_snap_position_ = SnapPosition::kPrimary;
    }
    // `other_window` must be set last, since we may have removed
    // `secondary_window_`.
    other_window = secondary_window_;
  } else if (snap_position == SnapPosition::kSecondary) {
    // See above comments.
    if (secondary_window_ != window) {
      previous_snapped_window = secondary_window_;
      StopObserving(SnapPosition::kSecondary);
      secondary_window_ = window;
    }
    if (primary_window_ == window) {
      primary_window_ = nullptr;
      default_snap_position_ = SnapPosition::kSecondary;
    }
    other_window = primary_window_;
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
        ->OnOverviewItemDragEnded(/*snap=*/true);
  }

  // Get the divider position given by `snap_ratio` if exists, or if there is
  // pre-set `divider_position_`, use it, which can happen during tablet <->
  // clamshell transition or multi-user transition. If neither `snap_ratio` nor
  // `divider_position_` exists, calculate the divider position with the default
  // snap ratio i.e. `chromeos::kDefaultSnapRatio`.
  // TODO(michelefan): See if it is a valid case to not having `snap_ratio`
  // while `divider_position` is less than 0.
  bool do_snap_animation = false;
  int divider_position =
      split_view_divider_.divider_widget() ? GetDividerPosition() : -1;
  if (std::optional<float> snap_ratio = WindowState::Get(window)->snap_ratio();
      snap_ratio) {
    divider_position = CalculateDividerPosition(
        root_window_, snap_position, *snap_ratio, ShouldConsiderDivider());
    // If `other_window` can't fit in the requested snap ratio, show a snap
    // animation below.
    do_snap_animation =
        other_window && !CanSnapWindow(other_window, 1.f - *snap_ratio);
  } else if (divider_position < 0) {
    divider_position = CalculateDividerPosition(root_window_, snap_position,
                                                chromeos::kDefaultSnapRatio,
                                                ShouldConsiderDivider());
  }

  // In clamshell mode we simply update `divider_position`. In tablet mode we
  // will show the divider widget below.
  split_view_divider_.SetDividerPosition(divider_position);
  base::RecordAction(base::UserMetricsAction("SplitView_SnapWindow"));
  if (!InTabletMode()) {
    return;
  }

  split_view_divider_.SetVisible(true);
  CHECK(split_view_divider_.HasDividerWidget());

  const int fixed_divider_position =
      GetClosestFixedDividerPosition(divider_position);
  // This must be done before we update `split_view_divider_.divider_position_`
  // to `fixed_divider_position`, since the minimum size will be respected
  // there.
  if (do_snap_animation) {
    // When `window` is re-snapped, i.e. from 1/2 to 2/3, but `other_window`
    // can't fit in the requested snap ratio, set `divider_snap_animation_` to
    // Hide then Show, to give off the impression of bouncing the divider back
    // to `old_divider_position`. Note the duration is 2 *
    // `kBouncingAnimationOneWayDuration` to bounce out then in.
    tablet_resize_mode_ = TabletResizeMode::kFast;
    divider_snap_animation_ = std::make_unique<DividerSnapAnimation>(
        this, /*starting_position=*/divider_position,
        /*ending_position=*/fixed_divider_position,
        2 * kBouncingAnimationOneWayDuration, gfx::Tween::FAST_OUT_SLOW_IN_3);
    divider_snap_animation_->Hide();
    divider_snap_animation_->Show();
  }

  split_view_divider_.SetDividerPosition(fixed_divider_position);
}

aura::Window* SplitViewController::GetSnappedWindow(SnapPosition position) {
  DCHECK_NE(SnapPosition::kNone, position);
  return position == SnapPosition::kPrimary ? primary_window_.get()
                                            : secondary_window_.get();
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
  gfx::Rect bounds =
      GetSnappedWindowBoundsInScreen(snap_position, window_for_minimum_size,
                                     snap_ratio, ShouldConsiderDivider());
  wm::ConvertRectFromScreen(root_window_, &bounds);
  return bounds;
}

bool SplitViewController::ShouldConsiderDivider() const {
  // The divider may be visible in tablet mode, or between two windows in a
  // snap group in clamshell mode.
  // In tablet mode, we always consider the divider width even if
  // `split_view_divider_` is not initialized yet because
  // `ClientControlledState` may need to know the snapped bounds before
  // actually snapping the windows.
  // TODO(b/309856199): Currently need to pipe `account_for_divider_width` since
  // the divider is still created in `SplitViewController` for Snap Groups.
  // Refactor this when `split_view_divider_` is moved out.
  return split_view_divider_.HasDividerWidget() || InTabletMode();
}

bool SplitViewController::IsDividerAnimating() const {
  return divider_snap_animation_ && divider_snap_animation_->is_animating();
}

void SplitViewController::EndSplitView(EndReason end_reason) {
  if (!InSplitViewMode()) {
    return;
  }

  end_reason_ = end_reason;

  // If we are currently in a resize but split view is ending, make sure to end
  // the resize. This can happen, for example, on the transition back to
  // clamshell mode or when a task is minimized during a resize. Likewise, if
  // split view is ending during the divider snap animation, then clean that up.
  // But if the split view is ending due to the destroy of `root_window_`, we
  // should skip the resize.
  const bool is_divider_animating = IsDividerAnimating();
  if ((IsResizingWithDivider() || is_divider_animating) &&
      end_reason != EndReason::kRootWindowDestroyed) {
    if (is_divider_animating) {
      // Don't call StopAndShoveAnimatedDivider as it will call observers.
      StopSnapAnimation();
    }
    EndResizeWithDividerImpl();
  }

  // There is at least one case where this line of code is needed: if the user
  // presses Ctrl+W while resizing a clamshell split view window.
  presentation_time_recorder_.reset();

  // Remove observers when the split view mode ends.
  Shell* shell = Shell::Get();
  shell->RemoveShellObserver(this);
  OverviewController::Get()->RemoveObserver(this);
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
  shell->activation_client()->RemoveObserver(this);

  auto_snap_controller_.reset();

  if (end_reason != EndReason::kRootWindowDestroyed) {
    // TODO(http://b/343542206): Break down the
    // `SplitViewOverviewSessionExitPoint` enum in
    // `SplitViewController::EndSplitView()`.
    const SplitViewOverviewSessionExitPoint exit_point =
        end_reason == EndReason::kSnapGroups
            ? SplitViewOverviewSessionExitPoint::kCompleteByActivating
            : SplitViewOverviewSessionExitPoint::kShutdown;

    // `EndSplitView()` is also called upon `~RootWindowController()` and
    // `~SplitViewController()`, during which `root_window_` would have been
    // destroyed.
    RootWindowController::ForWindow(root_window_)
        ->EndSplitViewOverviewSession(exit_point);
  }

  StopObserving(SnapPosition::kPrimary);
  StopObserving(SnapPosition::kSecondary);
  black_scrim_layer_.reset();
  default_snap_position_ = SnapPosition::kNone;
  divider_closest_ratio_ = std::numeric_limits<float>::quiet_NaN();
  snapping_window_transformed_bounds_map_.clear();

  UpdateStateAndNotifyObservers();

  // Close splitview divider widget after updating state so that
  // OnDisplayMetricsChanged triggered by the widget closing correctly
  // finds out !InSplitViewMode().
  split_view_divider_.SetVisible(false);
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
  if (!CanSnapWindow(target_window, chromeos::kDefaultSnapRatio)) {
    ShowAppCannotSnapToast();
    return;
  }

  // Save the overview enter/exit types to be used if the window is snapped.
  overview_start_action_ = OverviewStartAction::kOverviewButtonLongPress;
  enter_exit_overview_type_ = OverviewEnterExitType::kImmediateEnter;
  SnapWindow(target_window, SnapPosition::kPrimary,
             WindowSnapActionSource::kLongPressOverviewButtonToSnap,
             /*activate_window=*/true);

  base::RecordAction(
      base::UserMetricsAction("Tablet_LongPressOverviewButtonEnterSplitView"));
}

void SplitViewController::OnWindowDragStarted(aura::Window* dragged_window) {
  DCHECK(dragged_window);

  // OnSnappedWindowDetached() may end split view mode.
  if (IsWindowInSplitView(dragged_window)) {
    OnSnappedWindowDetached(dragged_window,
                            WindowDetachedReason::kWindowDragged);
  }

  if (split_view_divider_.divider_widget()) {
    split_view_divider_.OnWindowDragStarted(dragged_window);
  }
}

void SplitViewController::OnWindowDragEnded(
    aura::Window* dragged_window,
    SnapPosition desired_snap_position,
    const gfx::Point& last_location_in_screen,
    WindowSnapActionSource snap_action_source) {
  DCHECK(!window_util::IsDraggingTabs(dragged_window));
  EndWindowDragImpl(dragged_window, dragged_window->is_destroying(),
                    desired_snap_position, last_location_in_screen,
                    snap_action_source);
}

void SplitViewController::OnWindowDragCanceled() {
  if (split_view_divider_.divider_widget()) {
    split_view_divider_.OnWindowDragEnded();
  }
}

SnapPosition SplitViewController::ComputeSnapPosition(
    const gfx::Point& last_location_in_screen) {
  const int divider_position =
      InSplitViewMode()
          ? GetDividerPosition()
          : CalculateDividerPosition(root_window_, SnapPosition::kPrimary,
                                     chromeos::kDefaultSnapRatio,
                                     ShouldConsiderDivider());
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
  return changing_bounds_by_vk_ &&
         window == (IsLayoutPrimary(window) ? secondary_window_.get()
                                            : primary_window_.get());
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

  if (CanKeepCurrentSnapRatio(window)) {
    return;
  }

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
  if (!InClamshellSplitViewMode() || split_view_divider_.divider_widget()) {
    // Divider width is not taken into consideration in the calculation below.
    // Early exit if `split_view_divider_` exists in clamshell mode.
    return;
  }

  if (WindowState* window_state = WindowState::Get(window);
      window_state->is_dragged()) {
    if (presentation_time_recorder_) {
      presentation_time_recorder_->RequestNext();
    }
  }

  // During clamshell split view, we resize the window + overview at the same
  // time. `overview_utils` will do the work to calculate the overview grid
  // bounds from the window snapped in split view.
}

void SplitViewController::OnWindowDestroyed(aura::Window* window) {
  DCHECK(InSplitViewMode());
  DCHECK(IsWindowInSplitView(window));

  OnSnappedWindowDetached(window, WindowDetachedReason::kWindowDestroyed);
}

void SplitViewController::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  if (new_root) {
    // Detach the window first to stop ongoing divider animations.
    OnSnappedWindowDetached(window,
                            WindowDetachedReason::kWindowMovedToAnotherDisplay);
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
    OnWindowSnapped(window, old_type,
                    window_state->snap_action_source().value_or(
                        WindowSnapActionSource::kNotSpecified));
  } else if (window_state->IsNormalStateType() || window_state->IsMaximized() ||
             window_state->IsFullscreen() || window_state->IsFloated()) {
    // End split view, and also overview if overview is active, in these cases:
    // 1. A left clamshell split view window gets unsnapped by Alt+[.
    // 2. A right clamshell split view window gets unsnapped by Alt+].
    // 3. A (clamshell or tablet) split view window gets maximized.
    // 4. A (clamshell or tablet) split view window becomes full screen.
    // 5. A split view window becomes floated.
    EndSplitView();
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEndAction::kSplitView);
  } else if (window_state->IsMinimized()) {
    OnSnappedWindowDetached(window, WindowDetachedReason::kWindowMinimized);

    if (!InSplitViewMode()) {
      // We have different behaviors for a minimized window: in tablet splitview
      // mode, we'll insert the minimized window back to overview, as normally
      // the window is not supposed to be minmized in tablet mode. And in
      // clamshell splitview mode, we respect the minimization of the window
      // and end overview instead.
      if (InTabletMode()) {
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
  CHECK(InSplitViewMode());

  // While in clamshell split view mode without being in a snap group
  // creation session, a full overview session should be triggered. In this
  // case, split view should end.
  if (InClamshellSplitViewMode() &&
      !RootWindowController::ForWindow(root_window_)
           ->split_view_overview_session()) {
    EndSplitView();
    return;
  }

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

  // If clamshell split view mode is active, bail out. `OnOverviewModeEnded`
  // will end split view. We do not end split view here, because that would mess
  // up histograms of overview exit animation smoothness.
  if (!InTabletMode()) {
    return;
  }

  // Tablet split view mode is active. If it still only has one snapped window,
  // snap the first snappable window in the overview grid on the other side.
  if (state_ == State::kBothSnapped) {
    return;
  }

  OverviewGrid* current_grid =
      overview_session->GetGridWithRootWindow(root_window_);
  if (!current_grid || current_grid->empty()) {
    return;
  }

  for (const auto& overview_item : current_grid->item_list()) {
    for (aura::Window* window : overview_item->GetWindows()) {
      CHECK(window);

      if (window == GetDefaultSnappedWindow()) {
        continue;
      }

      std::optional<float> snap_ratio = ComputeAutoSnapRatio(window);
      if (!snap_ratio.has_value()) {
        continue;
      }

      const bool was_active =
          overview_session->IsWindowActiveWindowBeforeOverview(window);
      // Remove the overview item before snapping because the overview session
      // is unavailable to retrieve outside this function after
      // OnOverviewEnding is notified.
      overview_item->RestoreWindow(/*reset_transform=*/false,
                                   /*animate=*/true);
      overview_session->RemoveItem(overview_item.get());

      SnapWindow(window,
                 (default_snap_position_ == SnapPosition::kPrimary)
                     ? SnapPosition::kSecondary
                     : SnapPosition::kPrimary,
                 WindowSnapActionSource::kAutoSnapInSplitView,
                 /*activate_window=*/false, *snap_ratio);
      if (was_active) {
        wm::ActivateWindow(window);
      }

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
  if (DesksController::Get()->AreDesksBeingModified()) {
    return;
  }

  EndSplitView();
  ShowAppCannotSnapToast();
}

void SplitViewController::OnOverviewModeEnded() {
  DCHECK(InSplitViewMode());
  if (InClamshellSplitViewMode()) {
    EndSplitView();
  }
}

void SplitViewController::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  // If the `root_window_`is the root window of the display which is going to
  // be removed, there's no need to start overview.
  if (GetRootWindowSettings(root_window_)->display_id ==
      display::kInvalidDisplayId) {
    // Explicitly destroy the metrics controller here. If the display is removed
    // while a desk switch is in progress, the metrics controller will try to
    // access the non-existent root window in its desk activation obserer. Note
    // that `this` is soon going to be destroyed anyway.
    split_view_metrics_controller_.reset();
    return;
  }

  // If we are in tablet split view with only one snapped window, make sure we
  // are in overview (see https://crbug.com/1027179).
  if (state_ == State::kPrimarySnapped || state_ == State::kSecondarySnapped) {
    aura::Window* window =
        primary_window_ ? primary_window_ : secondary_window_;
    // `WindowSnapActionSource::kNotSpecified` is used as the snap source since
    // this is not user-initiated action.
    RootWindowController::ForWindow(window)->StartSplitViewOverviewSession(
        window, OverviewStartAction::kSplitView,
        OverviewEnterExitType::kImmediateEnter,
        WindowSnapActionSource::kNotSpecified);
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
  // directly if `IsUserSessionBlocked()`.
  if ((primary_window_ && !CanKeepCurrentSnapRatio(primary_window_)) ||
      (secondary_window_ && !CanKeepCurrentSnapRatio(secondary_window_))) {
    if (!Shell::Get()->session_controller()->IsUserSessionBlocked())
      EndSplitView();
    return;
  }

  // In clamshell split view mode, the divider position will be adjusted in
  // `OnWindowBoundsChanged`. Also when we first enter tablet mode, the divider
  // has not been created yet but there may be a work area change.
  if (!InTabletMode() || !split_view_divider_.divider_widget()) {
    return;
  }

  // Before adjusting the divider position for the new display metrics, if the
  // divider is animating to a snap position, then stop it and shove it there.
  // Postpone `EndSplitViewAfterResizingAtEdgeIfAppropriate()` until after the
  // adjustment, because the new display metrics will be used to compare the
  // divider position against the edges of the screen.
  if (IsDividerAnimating()) {
    StopAndShoveAnimatedDivider();
    EndResizeWithDividerImpl();
  }

  // If we ended split view in `EndSplitViewAfterResizingAtEdgeIfAppropriate()`,
  // no need to update the divider position.
  // TODO(b/329325825): Consider refactoring clamshell display change here.
  if (!InTabletSplitViewMode()) {
    return;
  }

  if ((metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION) ||
      (metrics & display::DisplayObserver::DISPLAY_METRIC_WORK_AREA)) {
    // Set default `divider_closest_ratio_` to kFixedPositionRatios[1].
    if (std::isnan(divider_closest_ratio_))
      divider_closest_ratio_ = kFixedPositionRatios[1];

    // Reverse the position ratio if top/left window changes.
    if (is_previous_layout_right_side_up != IsLayoutPrimary(display))
      divider_closest_ratio_ = 1.f - divider_closest_ratio_;
    split_view_divider_.SetDividerPosition(
        static_cast<int>(divider_closest_ratio_ *
                         GetDividerPositionUpperLimit(root_window_)) -
        kSplitviewDividerShortSideLength / 2);
  }

  // For other display configuration changes, we only move the divider to the
  // closest fixed position.
  if (!IsResizingWithDivider()) {
    split_view_divider_.SetDividerPosition(
        GetClosestFixedDividerPosition(GetDividerPosition()));
  }

  EndSplitViewAfterResizingAtEdgeIfAppropriate();
  NotifyDividerPositionChanged();
  UpdateSnappedWindowsAndDividerBounds();
}

void SplitViewController::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kInClamshellMode:
      OnTabletModeEnded();
      break;
    case display::TabletState::kEnteringTabletMode:
      break;
    case display::TabletState::kInTabletMode:
      OnTabletModeStarted();
      break;
    case display::TabletState::kExitingTabletMode:
      OnTabletModeEnding();
      break;
  }
}

void SplitViewController::OnAccessibilityStatusChanged() {
  // TODO(crubg.com/853588): Exit split screen if ChromeVox is turned on until
  // they are compatible.
  if (InTabletMode() &&
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    EndSplitView();
  }
}

void SplitViewController::OnAccessibilityControllerShutdown() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

void SplitViewController::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& screen_bounds) {
  // The window only needs to be moved if it is in the portrait mode.
  if (IsLayoutHorizontal(root_window_))
    return;

  // We only modify the bottom window if there is one and the current active
  // input field is in the bottom window.
  aura::Window* bottom_window = GetPhysicallyRightOrBottomWindow();
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
  if (!text_input_client) {
    return;
  }

  const gfx::Rect caret_bounds = text_input_client->GetCaretBounds();
  if (caret_bounds == gfx::Rect()) {
    return;
  }

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

  // Set bottom window bounds.
  {
    base::AutoReset<bool> enable_bounds_change(&changing_bounds_by_vk_, true);
    bottom_window->SetBoundsInScreen(
        bottom_bounds,
        display::Screen::GetScreen()->GetDisplayNearestWindow(root_window_));
  }

  split_view_divider_.OnKeyboardOccludedBoundsChangedInPortrait(work_area, y);
}

void SplitViewController::OnWindowActivated(ActivationReason reason,
                                            aura::Window* gained_active,
                                            aura::Window* lost_active) {
  // If the bottom window is moved for the virtual keyboard (the split view
  // divider bar is unadjustable), when the bottom window lost active, restore
  // to the original layout.
  if (!split_view_divider_.divider_widget() ||
      split_view_divider_.IsAdjustable()) {
    return;
  }

  if (IsLayoutHorizontal(root_window_)) {
    return;
  }

  aura::Window* bottom_window = GetPhysicallyRightOrBottomWindow();
  if (!bottom_window)
    return;

  if (bottom_window->Contains(lost_active) &&
      !bottom_window->Contains(gained_active)) {
    UpdateSnappedWindowsAndDividerBounds();
  }
}

aura::Window* SplitViewController::GetRootWindow() const {
  return root_window_;
}

void SplitViewController::StartResizeWithDivider(
    const gfx::Point& location_in_screen) {
  base::RecordAction(base::UserMetricsAction("SplitView_ResizeWindows"));
  if (state_ == State::kBothSnapped) {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        split_view_divider_.divider_widget()->GetCompositor(),
        kTabletSplitViewResizeMultiHistogram,
        kTabletSplitViewResizeMultiMaxLatencyHistogram);
    return;
  }
  // An ARC++ window may have snapped window state but not started overview yet.
  if (!IsInOverviewSession()) {
    return;
  }
  if (GetOverviewSession()->GetGridWithRootWindow(root_window_)->empty()) {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        split_view_divider_.divider_widget()->GetCompositor(),
        kTabletSplitViewResizeSingleHistogram,
        kTabletSplitViewResizeSingleMaxLatencyHistogram);
  } else {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        split_view_divider_.divider_widget()->GetCompositor(),
        kTabletSplitViewResizeWithOverviewHistogram,
        kTabletSplitViewResizeWithOverviewMaxLatencyHistogram);
  }
  accumulated_drag_time_ticks_ = base::TimeTicks::Now();
  accumulated_drag_distance_ = 0;

  tablet_resize_mode_ = TabletResizeMode::kNormal;
}

void SplitViewController::UpdateResizeWithDivider(
    const gfx::Point& location_in_screen) {
  // This updates `tablet_resize_mode_` based on drag speed.
  UpdateTabletResizeMode(base::TimeTicks::Now(), location_in_screen);

  NotifyDividerPositionChanged();
  UpdateSnappedWindowsBounds();

  // Update the resize backdrop, as well as the black scrim layer's bounds and
  // opacity.
  // TODO(b/298515546): Add performant resizing pattern.
  UpdateResizeBackdrop();
  UpdateBlackScrim(location_in_screen);

  // Apply window transform if necessary.
  SetWindowsTransformDuringResizing();
}

bool SplitViewController::EndResizeWithDivider(
    const gfx::Point& location_in_screen) {
  NotifyDividerPositionChanged();

  // Need to update snapped windows bounds even if the split view mode may have
  // to exit. Otherwise it's possible for a snapped window stuck in the edge of
  // of the screen while overview mode is active.
  UpdateSnappedWindowsBounds();
  NotifyWindowResized();

  presentation_time_recorder_.reset();

  // TODO(xdai): Use fade out animation instead of just removing it.
  black_scrim_layer_.reset();

  resize_timer_.Stop();
  tablet_resize_mode_ = TabletResizeMode::kNormal;

  const int divider_position = GetDividerPosition();
  const int target_divider_position =
      GetClosestFixedDividerPosition(divider_position);
  // TODO(b/298515283): Separate Snap Group and tablet resize.
  if (divider_position == target_divider_position ||
      IsSnapGroupEnabledInClamshellMode()) {
    return true;
  }
    divider_snap_animation_ = std::make_unique<DividerSnapAnimation>(
        this, divider_position, target_divider_position,
        base::Milliseconds(300), gfx::Tween::EASE_IN);
    divider_snap_animation_->Show();
    return false;
}

void SplitViewController::OnResizeEnding() {
  CHECK(InSplitViewMode());

  // The backdrop layers are removed here (rather than in
  // `EndResizeWithDivider()`) since they may be used while the divider is
  // animating to a snapped position.
  left_resize_backdrop_layer_.reset();
  right_resize_backdrop_layer_.reset();

  // Resize may not end with `EndResizeWithDivider()`, so make sure to clear
  // here too.
  resize_timer_.Stop();
  presentation_time_recorder_.reset();
  RestoreWindowsTransformAfterResizing();
}

void SplitViewController::OnResizeEnded() {
  EndSplitViewAfterResizingAtEdgeIfAppropriate();
}

void SplitViewController::SwapWindows() {
  DCHECK(InSplitViewMode());

  // Ignore `IsResizingWithDivider()` because it will be true in case of
  // double tapping (not double clicking) the divider without ever actually
  // dragging it anywhere. Double tapping the divider triggers
  // StartResizeWithDivider(), EndResizeWithDivider(), StartResizeWithDivider(),
  // SwapWindows(), EndResizeWithDivider(). Double clicking the divider
  // (possible by using the emulator or chrome://flags/#force-tablet-mode)
  // triggers StartResizeWithDivider(), EndResizeWithDivider(),
  // StartResizeWithDivider(), EndResizeWithDivider(), SwapWindows(). Those two
  // sequences of function calls are what were mainly considered in writing the
  // condition for bailing out here, to disallow swapping windows when the
  // divider is being dragged or is animating.
  if (IsDividerAnimating()) {
    return;
  }

  SwapWindowsAndUpdateBounds();
  if (IsSnapped(primary_window_)) {
    TriggerWMEventToSnapWindow(WindowState::Get(primary_window_),
                               WM_EVENT_SNAP_PRIMARY);
  }
  if (IsSnapped(secondary_window_)) {
    TriggerWMEventToSnapWindow(WindowState::Get(secondary_window_),
                               WM_EVENT_SNAP_SECONDARY);
  }

  // Update `default_snap_position_` if necessary.
  if (!primary_window_ || !secondary_window_) {
    default_snap_position_ =
        primary_window_ ? SnapPosition::kPrimary : SnapPosition::kSecondary;
  }

  split_view_divider_.SetDividerPosition(
      GetClosestFixedDividerPosition(GetDividerPosition()));
  UpdateStateAndNotifyObservers();
  NotifyWindowSwapped();

  base::RecordAction(
      base::UserMetricsAction("SplitView_DoubleTapDividerSwapWindows"));
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInScreen(
    SnapPosition snap_position,
    aura::Window* window_for_minimum_size,
    float snap_ratio,
    bool account_for_divider_width) const {
  if (snap_position == SnapPosition::kNone) {
    return screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
        root_window_);
  }

  const bool should_use_window_bounds_in_fast_resize =
      g_use_fast_resize_for_testing ||
      (IsResizingWithDivider() &&
       tablet_resize_mode_ == TabletResizeMode::kFast);
  if (window_for_minimum_size && should_use_window_bounds_in_fast_resize) {
    gfx::Rect bounds_in_screen(window_for_minimum_size->GetTargetBounds());
    wm::ConvertRectToScreen(root_window_, &bounds_in_screen);
    return bounds_in_screen;
  }

  const int divider_position =
      split_view_divider_.HasDividerWidget()
          ? GetDividerPosition()
          : CalculateDividerPosition(root_window_, snap_position, snap_ratio,
                                     account_for_divider_width);
  return CalculateSnappedWindowBoundsInScreen(
      snap_position, root_window_, window_for_minimum_size,
      account_for_divider_width, divider_position, IsResizingWithDivider());
}

SnapPosition SplitViewController::GetPositionOfSnappedWindow(
    const aura::Window* window) const {
  DCHECK(IsWindowInSplitView(window));
  return window == primary_window_ ? SnapPosition::kPrimary
                                   : SnapPosition::kSecondary;
}

void SplitViewController::SetUseFastResizeForTesting(bool val) {
  g_use_fast_resize_for_testing = val;
}

aura::Window* SplitViewController::GetPhysicallyLeftOrTopWindow() {
  DCHECK(root_window_);
  return IsLayoutPrimary(root_window_) ? primary_window_.get()
                                       : secondary_window_.get();
}

aura::Window* SplitViewController::GetPhysicallyRightOrBottomWindow() {
  DCHECK(root_window_);
  return IsLayoutPrimary(root_window_) ? secondary_window_.get()
                                       : primary_window_.get();
}

void SplitViewController::StartObserving(aura::Window* window) {
  if (window && !window->HasObserver(this)) {
    Shell::Get()->shadow_controller()->UpdateShadowForWindow(window);
    window->AddObserver(this);
    WindowState::Get(window)->AddObserver(this);
  }
  // Note `this` may already be observing `window`, but the divider isn't, i.e.
  // during clamshell <-> tablet split view transition.
  if (InTabletMode()) {
    split_view_divider_.MaybeAddObservedWindow(window);
  }
}

void SplitViewController::StopObserving(SnapPosition snap_position) {
  aura::Window* window = GetSnappedWindow(snap_position);
  if (window == primary_window_) {
    primary_window_ = nullptr;
  } else {
    secondary_window_ = nullptr;
  }

  if (window && window->HasObserver(this)) {
    window->RemoveObserver(this);
    WindowState::Get(window)->RemoveObserver(this);
    // Must be called after we reset `primary_window_|secondary_window_`.
    split_view_divider_.MaybeRemoveObservedWindow(window);
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
  const State previous_state = state_;
  if (IsSnapped(primary_window_) && IsSnapped(secondary_window_)) {
    state_ = State::kBothSnapped;
  } else if (IsSnapped(primary_window_)) {
    state_ = State::kPrimarySnapped;
  } else if (IsSnapped(secondary_window_)) {
    state_ = State::kSecondarySnapped;
  } else {
    state_ = State::kNoSnap;
  }

  // We still notify observers even if |state_| doesn't change as it's possible
  // to snap a window to a position that already has a snapped window. However,
  // |previous_state| and |state_| cannot both be |State::kNoSnap|.
  // When |previous_state| is |State::kNoSnap|, it indicates to
  // observers that split view mode started. Likewise, when |state_| is
  // |State::kNoSnap|, it indicates to observers that split view mode
  // ended.
  DCHECK(previous_state != State::kNoSnap || state_ != State::kNoSnap ||
         end_reason_ == EndReason::kSnapGroups);
  for (auto& observer : observers_) {
    observer.OnSplitViewStateChanged(previous_state, state_);
  }
  const bool was_in_split_view = previous_state != State::kNoSnap;
  const bool is_in_split_view = InSplitViewMode();
  // If we just started split view, add observers. Don't do this unless we just
  // started split view to avoid adding observers twice.
  if (!was_in_split_view && is_in_split_view) {
    Shell* shell = Shell::Get();
    // Add observers when the split view mode starts.
    shell->AddShellObserver(this);
    OverviewController::Get()->AddObserver(this);
    keyboard::KeyboardUIController::Get()->AddObserver(this);
    shell->activation_client()->AddObserver(this);

    auto_snap_controller_ = std::make_unique<AutoSnapController>(root_window_);
  }
  // If we are ending split view, ensure that `EndSplitView()` has been called
  // to remove the observers and `auto_snap_controller_` has been destroyed.
  // Note this assumes that `auto_snap_controller_` is always destroyed in the
  // same scope that observers are removed.
  if (was_in_split_view && !is_in_split_view) {
    CHECK(!auto_snap_controller_);
  }
}

void SplitViewController::NotifyDividerPositionChanged() {
  for (auto& observer : observers_) {
    observer.OnSplitViewDividerPositionChanged();
  }
}

void SplitViewController::NotifyWindowResized() {
  for (auto& observer : observers_) {
    observer.OnSplitViewWindowResized();
  }
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
    auto* divider_layer = split_view_divider_.GetDividerWindow()->layer();
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
      position, /*window_for_minimum_size=*/nullptr,
      chromeos::kDefaultSnapRatio, ShouldConsiderDivider()));

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
  const float ratio = chromeos::kOneThirdSnapRatio - kBlackScrimFadeInRatio;
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
    backdrop->SetBounds(GetSnappedWindowBoundsInParent(
        position, nullptr, chromeos::kDefaultSnapRatio));
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

void SplitViewController::UpdateSnappedWindowBounds(aura::Window* window) {
  DCHECK(IsWindowInSplitView(window));
  WindowState* window_state = WindowState::Get(window);
  if (InTabletMode()) {
    if (window_state->is_client_controlled()) {
      // TODO(b/264962634): Remove this workaround. Probably, we can rewrite
      // `TabletModeWindowState::UpdateWindowPosition` to include this logic.
      const gfx::Rect requested_bounds =
          TabletModeWindowState::GetBoundsInTabletMode(window_state);
      const SetBoundsWMEvent event(requested_bounds,
                                   /*animate=*/true);
      window_state->OnWMEvent(&event);
    } else {
      TabletModeWindowState::UpdateWindowPosition(
          window_state, WindowState::BoundsChangeAnimationType::kAnimate);
    }
  } else {
    const gfx::Rect requested_bounds = GetSnappedWindowBoundsInParent(
        GetPositionOfSnappedWindow(window), window,
        window_util::GetSnapRatioForWindow(window));
    const SetBoundsWMEvent event(requested_bounds, /*animate=*/true);
    window_state->OnWMEvent(&event);
  }
}

void SplitViewController::UpdateSnappedWindowsBounds() {
  // Update the snapped windows' bounds. If the window is already snapped in the
  // correct position, simply update the snap ratio.
  if (IsSnapped(primary_window_)) {
    UpdateSnappedWindowBounds(primary_window_);
  }
  if (IsSnapped(secondary_window_)) {
    UpdateSnappedWindowBounds(secondary_window_);
  }
}

void SplitViewController::UpdateSnappedWindowsAndDividerBounds() {
  UpdateSnappedWindowsBounds();

  // Update divider's bounds and make it adjustable.
  if (split_view_divider_.divider_widget()) {
    split_view_divider_.UpdateDividerBounds();

    // Make the split view divider adjustable.
    split_view_divider_.SetAdjustable(true);
  }
}

SnapPosition SplitViewController::GetBlackScrimPosition(
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
  int divider_upper_limit = GetDividerPositionUpperLimit(root_window_);
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

  if (primary_window_distance <
          divider_upper_limit * chromeos::kOneThirdSnapRatio ||
      primary_window_distance < min_left_length) {
    return SnapPosition::kPrimary;
  }
  if (secondary_window_distance <
          divider_upper_limit * chromeos::kOneThirdSnapRatio ||
      secondary_window_distance < min_right_length) {
    return SnapPosition::kSecondary;
  }

  return SnapPosition::kNone;
}

int SplitViewController::GetClosestFixedDividerPosition(int divider_position) {
  // The values in |kFixedPositionRatios| represent the fixed position of the
  // center of the divider while |GetDividerPosition()| represent the origin of
  // the divider rectangle. So, before calling FindClosestFixedPositionRatio,
  // extract the center from |GetDividerPosition()|. The result will also be the
  // center of the divider, so extract the origin, unless the result is on of
  // the endpoints.
  int divider_upper_limit = GetDividerPositionUpperLimit(root_window_);
  // TODO(b/319334795): Move this function and `divider_closest_ratio_` to
  // SplitViewDivider.
  divider_closest_ratio_ = FindClosestPositionRatio(
      float(divider_position + kSplitviewDividerShortSideLength / 2) /
      divider_upper_limit);
  int fixed_position = divider_upper_limit * divider_closest_ratio_;
  if (divider_closest_ratio_ > 0.f && divider_closest_ratio_ < 1.f) {
    fixed_position -= kSplitviewDividerShortSideLength / 2;
  }
  return std::clamp(fixed_position, 0, divider_upper_limit);
}

void SplitViewController::StopAndShoveAnimatedDivider() {
  CHECK(IsDividerAnimating());

  StopSnapAnimation();
  NotifyDividerPositionChanged();
  UpdateSnappedWindowsAndDividerBounds();
}

void SplitViewController::StopSnapAnimation() {
  divider_snap_animation_->Stop();
  split_view_divider_.SetDividerPosition(
      divider_snap_animation_->ending_position());
}

bool SplitViewController::ShouldEndSplitViewAfterResizingAtEdge() {
  if (!InTabletSplitViewMode()) {
    // `SplitViewDivider::CleanUpWindowResizing()` may be called after a display
    // change, after which we have ended split view.
    // TODO(sophiewen): Only call `SplitViewDivider::CleanUpWindowResizing()` if
    // we actually ended resizing.
    return false;
  }
  const int divider_position = GetDividerPosition();
  return divider_position == 0 ||
         divider_position == GetDividerPositionUpperLimit(root_window_);
}

void SplitViewController::EndSplitViewAfterResizingAtEdgeIfAppropriate() {
  if (!ShouldEndSplitViewAfterResizingAtEdge()) {
    return;
  }

  aura::Window* active_window = GetActiveWindowAfterResizingUponExit();

  // Track the window that needs to be put back into the overview list if we
  // remain in overview mode.
  aura::Window* insert_overview_window = nullptr;
  if (IsInOverviewSession()) {
    insert_overview_window = GetDefaultSnappedWindow();
  }

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

  if (!ShouldEndSplitViewAfterResizingAtEdge()) {
    return nullptr;
  }

  return GetDividerPosition() == 0 ? GetPhysicallyRightOrBottomWindow()
                                   : GetPhysicallyLeftOrTopWindow();
}

void SplitViewController::OnWindowSnapped(
    aura::Window* window,
    std::optional<chromeos::WindowStateType> previous_state,
    WindowSnapActionSource snap_action_source) {
  RestoreTransformIfApplicable(window);

  // We must add snap group and end split view before updating state.
  if (IsSnapGroupEnabledInClamshellMode()) {
    if (SnapGroupController::Get()->OnWindowSnapped(window,
                                                    snap_action_source)) {
      // Detaching the snapped window will end split view so no need to
      // update state and notify observers again.
      OnSnappedWindowDetached(window, WindowDetachedReason::kAddedToSnapGroup);
      CHECK(!InSplitViewMode());
      return;
    }
  }

  UpdateStateAndNotifyObservers();

  // If the snapped window was removed from overview and was the active window
  // before entering overview, it should be the active window after snapping
  // in splitview.
  if (to_be_activated_window_ == window) {
    to_be_activated_window_ = nullptr;
    wm::ActivateWindow(window);
  }

  // In tablet mode, if the window was previously floated, the other side is
  // available, and there is another non-minimized window, do not enter
  // overview but instead snap that window to the opposite side.
  if (InTabletMode() && previous_state &&
      *previous_state == chromeos::WindowStateType::kFloated &&
      state_ != State::kBothSnapped) {
    for (aura::Window* mru_window :
         Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(
             kActiveDesk)) {
      auto* window_state = WindowState::Get(mru_window);
      if (mru_window != window && !window_state->IsMinimized() &&
          window_state->CanSnap()) {
        const SnapPosition snap_position =
            GetPositionOfSnappedWindow(window) == SnapPosition::kPrimary
                ? SnapPosition::kSecondary
                : SnapPosition::kPrimary;
        WindowSnapWMEvent event(snap_position == SnapPosition::kPrimary
                                    ? WM_EVENT_SNAP_PRIMARY
                                    : WM_EVENT_SNAP_SECONDARY,
                                WindowSnapActionSource::kAutoSnapInSplitView);
        WindowState::Get(mru_window)->OnWMEvent(&event);
        return;
      }
    }
  }

  if (WillStartPartialOverview(window)) {
    if (!InTabletMode()) {
      base::RecordAction(
          base::UserMetricsAction("SnapGroups_StartPartialOverview"));
    }
    RootWindowController::ForWindow(window)->StartSplitViewOverviewSession(
        window, overview_start_action_, enter_exit_overview_type_,
        snap_action_source);
    overview_start_action_.reset();
    enter_exit_overview_type_.reset();
    return;
  }

  // If we are in clamshell and did *not* start partial overview, which may
  // happen if there is an opposite snapped window not in split view, end
  // split view, except for the following cases:
  // 1. Partial overview may already be in session, i.e. if the window snapped
  // in partial overview swaps snap positions via the window layout menu.
  // 2. During tablet -> clamshell transition, we do not end split view since
  // it may still be needed by `SnapGroupController` to create a `SnapGroup`.
  // Split view will be ended either in `MaybeCreateSnapGroup()` or
  // `MaybeEndSplitViewAndOverview()` in `TabletModeWindowManager`.
  // TODO(b/327269057): Refactor tablet <-> clamshell transition.
  if (!InTabletMode() &&
      !RootWindowController::ForWindow(window)->split_view_overview_session() &&
      snap_action_source !=
          WindowSnapActionSource::kSnapByClamshellTabletTransition) {
    base::RecordAction(
        base::UserMetricsAction("SnapGroups_SkipFormSnapGroupAfterSnapping"));
    EndSplitView(EndReason::kNormal);
    return;
  }

  UpdateSnappedWindowsAndDividerBounds();
}

void SplitViewController::OnSnappedWindowDetached(aura::Window* window,
                                                  WindowDetachedReason reason) {
  auto iter = snapping_window_transformed_bounds_map_.find(window);
  if (iter != snapping_window_transformed_bounds_map_.end()) {
    snapping_window_transformed_bounds_map_.erase(iter);
  }

  if (to_be_activated_window_ == window) {
    to_be_activated_window_ = nullptr;
  }

  const bool is_window_moved =
      reason == WindowDetachedReason::kWindowMovedToAnotherDisplay;
  const bool is_window_destroyed_or_moved =
      reason == WindowDetachedReason::kWindowDestroyed || is_window_moved;
  const SnapPosition position_of_snapped_window =
      GetPositionOfSnappedWindow(window);

  // Detach it from splitview first if the window is to be destroyed to prevent
  // unnecessary bounds/state update to it when ending splitview resizing. For
  // the window that is not going to be destroyed, we still need its bounds and
  // state to be updated to match the updated divider position before detaching
  // it from splitview.
  if (is_window_destroyed_or_moved) {
    StopObserving(position_of_snapped_window);
  }

  // Stop resizing if one of the snapped window is detached from split
  // view.
  const bool is_divider_animating = IsDividerAnimating();
  if (IsResizingWithDivider() || is_divider_animating) {
    if (is_divider_animating) {
      StopAndShoveAnimatedDivider();
    }
    EndResizeWithDividerImpl();
  }

  if (!is_window_destroyed_or_moved) {
    StopObserving(position_of_snapped_window);
  }

  // End the Split View mode for the following two cases:
  // 1. If there is no snapped window at this moment;
  // 2. In Clamshell partial overview, `SplitViewController` will no longer
  // manage the window on one window detached.
  auto should_end_split_view = [&]() -> bool {
    if (!primary_window_ && !secondary_window_) {
      return true;
    }

    return InClamshellSplitViewMode() &&
           (!primary_window_ || !secondary_window_);
  };
  if (should_end_split_view()) {
    EndReason end_reason = EndReason::kNormal;
    switch (reason) {
      case WindowDetachedReason::kWindowDragged:
        end_reason = EndReason::kWindowDragStarted;
        break;
      case WindowDetachedReason::kAddedToSnapGroup:
        end_reason = EndReason::kSnapGroups;
        break;
      default:
        break;
    }
    EndSplitView(end_reason);
    if (is_window_moved) {
      // If the snapped window is being moved to another display, end overview.
      Shell::Get()->overview_controller()->EndOverview(
          OverviewEndAction::kSplitView);
    }
  } else {
    DCHECK(InTabletSplitViewMode());
    aura::Window* other_window =
        GetSnappedWindow(position_of_snapped_window == SnapPosition::kPrimary
                             ? SnapPosition::kSecondary
                             : SnapPosition::kPrimary);

    if (reason == WindowDetachedReason::kWindowFloated || is_window_moved) {
      // Maximize the other window, which will end split view.
      WMEvent event(WM_EVENT_MAXIMIZE);
      WindowState::Get(other_window)->OnWMEvent(&event);
      return;
    }

    // If there is still one snapped window after minimizing/closing one snapped
    // window, update its snap state and open overview window grid.
    default_snap_position_ =
        primary_window_ ? SnapPosition::kPrimary : SnapPosition::kSecondary;
    UpdateStateAndNotifyObservers();
    // `WindowSnapActionSource::kNotSpecified` is used as the snap source since
    // this is not user-initiated action.
    RootWindowController::ForWindow(other_window)
        ->StartSplitViewOverviewSession(
            other_window, OverviewStartAction::kFasterSplitScreenSetup,
            reason == WindowDetachedReason::kWindowDragged
                ? OverviewEnterExitType::kImmediateEnter
                : OverviewEnterExitType::kNormal,
            WindowSnapActionSource::kNotSpecified);
  }
}

void SplitViewController::ModifyPositionRatios(
    std::vector<float>& out_position_ratios) {
  const bool landscape = IsCurrentScreenOrientationLandscape();
  const int min_left_size =
      GetMinimumWindowLength(GetPhysicallyLeftOrTopWindow(), landscape);
  const int min_right_size =
      GetMinimumWindowLength(GetPhysicallyRightOrBottomWindow(), landscape);
  const int divider_upper_limit = GetDividerPositionUpperLimit(root_window_);
  const float min_size_left_ratio =
      static_cast<float>(min_left_size) / divider_upper_limit;
  const float min_size_right_ratio =
      static_cast<float>(min_right_size) / divider_upper_limit;
  if (min_size_left_ratio > chromeos::kOneThirdSnapRatio) {
    // If `primary_window_` can't fit in 1/3, remove 0.33f divider position.
    std::erase(out_position_ratios, chromeos::kOneThirdSnapRatio);
  }
  if (min_size_right_ratio > chromeos::kOneThirdSnapRatio) {
    // If `secondary_window_` can't fit in 1/3, remove 0.67f divider position.
    std::erase(out_position_ratios, chromeos::kTwoThirdSnapRatio);
  }
  // Remove 0.5f if a window cannot be snapped. We can get into this state by
  // snapping a window to two thirds.
  if (min_size_left_ratio > chromeos::kDefaultSnapRatio ||
      min_size_right_ratio > chromeos::kDefaultSnapRatio) {
    std::erase(out_position_ratios, chromeos::kDefaultSnapRatio);
  }
}

float SplitViewController::FindClosestPositionRatio(float current_ratio) {
  float closest_ratio = 0.f;
  std::vector<float> position_ratios(
      kFixedPositionRatios,
      kFixedPositionRatios + std::size(kFixedPositionRatios));
  ModifyPositionRatios(position_ratios);
  float min_ratio_diff = std::numeric_limits<float>::max();
  for (const float ratio : position_ratios) {
    const float ratio_diff = std::abs(current_ratio - ratio);
    if (ratio_diff < min_ratio_diff) {
      min_ratio_diff = ratio_diff;
      closest_ratio = ratio;
    }
  }
  return closest_ratio;
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
        GetPositionOfSnappedWindow(window), window,
        window_util::GetSnapRatioForWindow(window), ShouldConsiderDivider());
    const gfx::Transform starting_transform = gfx::TransformBetweenRects(
        gfx::RectF(snapped_bounds), gfx::RectF(item_bounds));
    SetTransformWithAnimation(window, starting_transform, gfx::Transform());
  }
}

void SplitViewController::SetWindowsTransformDuringResizing() {
  CHECK(InTabletSplitViewMode() || IsSnapGroupEnabledInClamshellMode());
  const int divider_position = GetDividerPosition();
  CHECK_GE(divider_position, 0);
  aura::Window* left_or_top_window = GetPhysicallyLeftOrTopWindow();
  aura::Window* right_or_bottom_window = GetPhysicallyRightOrBottomWindow();
  if (left_or_top_window) {
    SetWindowTransformDuringResizing(left_or_top_window, divider_position);
  }
  if (right_or_bottom_window) {
    SetWindowTransformDuringResizing(right_or_bottom_window, divider_position);
  }
}

void SplitViewController::RestoreWindowsTransformAfterResizing() {
  DCHECK(InSplitViewMode());
  if (primary_window_)
    window_util::SetTransform(primary_window_, gfx::Transform());
  if (secondary_window_)
    window_util::SetTransform(secondary_window_, gfx::Transform());
  if (black_scrim_layer_.get()) {
    black_scrim_layer_->SetTransform(gfx::Transform());
  }
}

void SplitViewController::SetTransformWithAnimation(
    aura::Window* window,
    const gfx::Transform& start_transform,
    const gfx::Transform& target_transform) {
  for (auto* window_iter : GetTransientTreeIterator(window)) {
    // Adjust `start_transform` and `target_transform` for the transient child.
    const gfx::PointF target_origin =
        GetUnionScreenBoundsForWindow(window).origin();
    gfx::RectF original_bounds(window_iter->GetTargetBounds());
    wm::TranslateRectToScreen(window_iter->parent(), &original_bounds);
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
      OverviewController* overview_controller = OverviewController::Get();
      OverviewSession* overview_session =
          overview_controller->overview_session();
      if (overview_controller->IsCompletingShutdownAnimations() ||
          (overview_session && overview_session->is_shutting_down() &&
           overview_session->enter_exit_overview_type() !=
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

void SplitViewController::EndResizeWithDividerImpl() {
  split_view_divider_.CleanUpWindowResizing();
}

void SplitViewController::OnResizeTimer() {
  if (InSplitViewMode() && split_view_divider_.divider_widget()) {
    split_view_divider_.ResizeWithDivider(
        split_view_divider_.previous_event_location());
  }
}

void SplitViewController::UpdateTabletResizeMode(
    base::TimeTicks event_time_ticks,
    const gfx::Point& event_location) {
  if (!presentation_time_recorder_) {
    base::debug::DumpWithoutCrashing();
  } else {
    presentation_time_recorder_->RequestNext();
  }

  if (IsLayoutHorizontal(root_window_)) {
    accumulated_drag_distance_ += std::abs(
        event_location.x() - split_view_divider_.previous_event_location().x());
  } else {
    accumulated_drag_distance_ += std::abs(
        event_location.y() - split_view_divider_.previous_event_location().y());
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

  // If we are in the fast mode, start a timer that automatically invokes
  // `ResizeWithDivider()` after a timeout. This ensure that we can switch back
  // to the normal mode if the user stops dragging. Note: if the timer is
  // already active, this will simply move the deadline forward.
  if (tablet_resize_mode_ == TabletResizeMode::kFast) {
    resize_timer_.Start(FROM_HERE, kSplitViewChunkTime, this,
                        &SplitViewController::OnResizeTimer);
  }
}

void SplitViewController::OnTabletModeStarted() {
  is_previous_layout_right_side_up_ = IsCurrentScreenOrientationPrimary();
  // If splitview is active when tablet mode is starting, create the split view
  // divider if not exists and adjust the `GetDividerPosition()` to be one
  // of the fixed positions.
  if (InSplitViewMode()) {
    // The windows would already have been attached before transition, in
    // `TabletModeWindowManager::ArrangeWindowsForTabletMode()`.
    CHECK(primary_window_ || secondary_window_);
    // Take divider width into calculation since divider will always be
    // available for tablet split view. Tablet mode only supports the fixed
    // divider positions in `kFixedPositionRatios`, so push `divider_position_`
    // to the closest fixed ratio.
    const int divider_position =
        GetClosestFixedDividerPosition(GetEquivalentDividerPosition(
            primary_window_ ? primary_window_ : secondary_window_,
            /*account_for_divider_width=*/true));
    split_view_divider_.SetDividerPosition(divider_position);

    UpdateSnappedWindowsAndDividerBounds();
    NotifyDividerPositionChanged();

    // Ends `SplitViewOverviewSession` if it is currently alive, as
    // `SplitViewOverviewSession` is for clamshell only.
    RootWindowController* root_window_controller =
        RootWindowController::ForWindow(root_window_);
    if (root_window_controller->split_view_overview_session()) {
      root_window_controller->EndSplitViewOverviewSession(
          SplitViewOverviewSessionExitPoint::kTabletConversion);
    }
  }
}

void SplitViewController::OnTabletModeEnding() {
  // `OnTabletModeEnding()` can also be called during test teardown.
  const bool is_divider_animating = IsDividerAnimating();
  if (IsResizingWithDivider() || is_divider_animating) {
    if (is_divider_animating) {
      StopAndShoveAnimatedDivider();
    }

    EndResizeWithDividerImpl();
  }

  split_view_divider_.SetVisible(false);
}

void SplitViewController::OnTabletModeEnded() {
  is_previous_layout_right_side_up_ = true;
}

void SplitViewController::EndWindowDragImpl(
    aura::Window* window,
    bool is_being_destroyed,
    SnapPosition desired_snap_position,
    const gfx::Point& last_location_in_screen,
    WindowSnapActionSource snap_action_source) {
  if (split_view_divider_.divider_widget()) {
    split_view_divider_.OnWindowDragEnded();
  }

  // If the dragged window is to be destroyed, do not try to snap it.
  if (is_being_destroyed)
    return;

  // If dragged window was in overview before or it has been added to overview
  // window by dropping on the new selector item, do nothing.
  if (GetOverviewSession() && GetOverviewSession()->IsWindowInOverview(window))
    return;

  if (WindowState::Get(window)->IsFloated()) {
    // If a floated window was dragged from shelf and released, don't snap.
    return;
  }

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
                 snap_action_source,
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
    // Note SnapWindow() might put the previous window that was snapped at the
    // |desired_snap_position| in overview.
    SnapWindow(window, desired_snap_position, snap_action_source,
               /*activate_window=*/true);
  }
}

void SplitViewController::SwapWindowsAndUpdateBounds() {
  gfx::Rect primary_window_bounds =
      primary_window_ ? primary_window_->GetBoundsInScreen() : gfx::Rect();
  gfx::Rect secondary_window_bounds =
      secondary_window_ ? secondary_window_->GetBoundsInScreen() : gfx::Rect();
  aura::Window* cached_window = primary_window_;
  primary_window_ = secondary_window_;
  secondary_window_ = cached_window;

  const auto dst_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window_);

  if (primary_window_) {
    primary_window_->SetBoundsInScreen(secondary_window_bounds, dst_display);
  }

  if (secondary_window_) {
    secondary_window_->SetBoundsInScreen(primary_window_bounds, dst_display);
  }
}

}  // namespace ash

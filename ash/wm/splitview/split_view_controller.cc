// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_controller.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/app_types.h"
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
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/delayed_animation_observer_impl.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/snap_group/snap_group_controller.h"
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
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
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

using ::chromeos::WindowStateType;

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

WindowStateType GetStateTypeFromSnapPosition(
    SplitViewController::SnapPosition snap_position) {
  switch (snap_position) {
    case SplitViewController::SnapPosition::kPrimary:
      return WindowStateType::kPrimarySnapped;
    case SplitViewController::SnapPosition::kSecondary:
      return WindowStateType::kSecondarySnapped;
    default:
      NOTREACHED_NORETURN();
  }
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

// Returns true if |window| is currently snapped.
bool IsSnapped(aura::Window* window) {
  if (!window)
    return false;
  return WindowState::Get(window)->IsSnapped();
}

bool IsInTabletMode() {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  return tablet_mode_controller && tablet_mode_controller->InTabletMode();
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
bool DidInSplitViewWindowChange(
    aura::Window* window,
    SplitViewController* split_view_controller,
    SplitViewController::SnapPosition snap_position) {
  if (!split_view_controller->IsWindowInSplitView(window)) {
    return false;
  }

  const auto* window_state = WindowState::Get(window);
  if (window_state->GetStateType() !=
      GetStateTypeFromSnapPosition(snap_position)) {
    return true;
  }

  // For the current tablet mode split view design, we can assume that the
  // `window` is being snapped to the same `snap_position` it was snapped since
  // it's single layer design. We need to check if the snap ratio is the same.
  absl::optional<float> snap_ratio = window_state->snap_ratio();
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
    DCHECK(!split_view_controller_->IsResizingWithDivider());
    DCHECK_EQ(ending_position_, split_view_controller_->divider_position_);

    split_view_controller_->EndResizeWithDividerImpl();
    split_view_controller_->EndSplitViewAfterResizingAtEdgeIfAppropriate();

    if (tracker_)
      tracker_->Stop();
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    DCHECK(split_view_controller_->InSplitViewMode());
    DCHECK(!split_view_controller_->IsResizingWithDivider());

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

  raw_ptr<SplitViewController, ExperimentalAsh> split_view_controller_;
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
                            SplitViewController::SnapPosition snap_position,
                            WindowSnapActionSource snap_action_source) {
    if (DidInSplitViewWindowChange(window, split_view_controller_,
                                   snap_position)) {
      split_view_controller_->AttachSnappingWindow(window, snap_position,
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
        GetStateTypeFromSnapPosition(snap_position)) {
      split_view_controller_->AttachSnappingWindow(window, snap_position,
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
        GetStateTypeFromSnapPosition(snap_position)) {
      const auto cached_snap_action_source = iter->second.snap_action_source;
      to_be_snapped_windows_.erase(iter);
      window_state->RemoveObserver(this);
      window->RemoveObserver(this);
      split_view_controller_->AttachSnappingWindow(window, snap_position,
                                                   cached_snap_action_source);
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

  const raw_ptr<SplitViewController, ExperimentalAsh> split_view_controller_;

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

// static
bool SplitViewController::IsLayoutHorizontal(aura::Window* window) {
  return IsLayoutHorizontal(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
}

// static
bool SplitViewController::IsLayoutHorizontal(const display::Display& display) {
  if (IsInTabletMode()) {
    return IsCurrentScreenOrientationLandscape();
  }

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
  if (IsInTabletMode()) {
    return IsCurrentScreenOrientationPrimary();
  }

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

// -----------------------------------------------------------------------------
// SplitViewController:

SplitViewController::SplitViewController(aura::Window* root_window)
    : root_window_(root_window),
      to_be_snapped_windows_observer_(
          std::make_unique<ToBeSnappedWindowsObserver>(this)),
      split_view_metrics_controller_(
          std::make_unique<SplitViewMetricsController>(this)) {
  Shell::Get()->accessibility_controller()->AddObserver(this);
  if (SnapGroupController* snap_group_controller = SnapGroupController::Get()) {
    snap_group_controller->AddObserver(this);
  }
  split_view_type_ = IsInTabletMode() ? SplitViewType::kTabletType
                                      : SplitViewType::kClamshellType;
}

SplitViewController::~SplitViewController() {
  if (Shell::Get()->accessibility_controller()) {
    Shell::Get()->accessibility_controller()->RemoveObserver(this);
  }
  if (SnapGroupController* snap_group_controller = SnapGroupController::Get()) {
    snap_group_controller->RemoveObserver(this);
  }

  EndSplitView(EndReason::kRootWindowDestroyed);
}

int SplitViewController::divider_position() const {
  // TODO(b/308819668): Temporary check. Remove this entire function when
  // `divider_position_` is moved to `SplitViewDivider`.
  CHECK(split_view_divider_);
  return divider_position_;
}

bool SplitViewController::IsResizingWithDivider() const {
  return split_view_divider_ && split_view_divider_->is_resizing_with_divider();
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
  return CanSnapWindow(window, chromeos::kDefaultSnapRatio);
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

  return GetMinimumWindowLength(window, IsLayoutHorizontal(window)) <=
         GetDividerPositionUpperLimit() * snap_ratio -
             kSplitviewDividerShortSideLength / 2;
}

std::optional<float> SplitViewController::ComputeSnapRatio(
    aura::Window* window) {
  // If there is no default snapped window, or it doesn't have a stored snap
  // ratio try snapping it to 1/2.
  aura::Window* default_window = GetDefaultSnappedWindow();
  std::optional<float> default_window_snap_ratio =
      default_window ? WindowState::Get(default_window)->snap_ratio()
                     : std::nullopt;
  if (!default_window_snap_ratio) {
    return CanSnapWindow(window)
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
  auto* it = kOppositeRatiosMap.find(*default_window_snap_ratio);
  // TODO(sammiequon): Investigate if this check is needed. It may be needed for
  // rounding errors (i.e. 2/3 may be 0.67).
  if (it == kOppositeRatiosMap.end()) {
    return CanSnapWindow(window)
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
  if (snap_ratio == chromeos::kOneThirdSnapRatio && CanSnapWindow(window) &&
      CanSnapWindow(default_window)) {
    return chromeos::kDefaultSnapRatio;
  }

  return std::nullopt;
}

bool SplitViewController::WillStartOverview() const {
  // Note that at this point `state_` may not have been updated yet, so check if
  // only one of `primary_window_` or `secondary_window_` are snapped.
  return !IsInOverviewSession() && !DesksController::Get()->animation() &&
         split_view_type_ == SplitViewType::kTabletType &&
         !!primary_window_ != !!secondary_window_;
}

void SplitViewController::SnapWindow(aura::Window* window,
                                     SnapPosition snap_position,
                                     WindowSnapActionSource snap_action_source,
                                     bool activate_window,
                                     float snap_ratio) {
  DCHECK(window && CanSnapWindow(window));
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

  // In clamshell mode, only if overview is active on window snapped or when the
  // feature flag `kSnapGroup` is enabled and the feature param
  // `kAutomaticallyLockGroup` is true, the window should be managed by
  // `SplitViewController`. Otherwise, the window should be snapped normally and
  // should be managed by `WindowState`.
  if (split_view_type_ == SplitViewType::kClamshellType &&
      !(in_overview || IsSnapGroupEnabledInClamshellMode())) {
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
  // to override `divider_position_` from a `new_snap_ratio` below.
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

void SplitViewController::AttachSnappingWindow(
    aura::Window* window,
    SnapPosition snap_position,
    WindowSnapActionSource snap_action_source) {
  // Save the transformed bounds in preparation for the snapping animation.
  UpdateSnappingWindowTransformedBounds(window);

  OverviewSession* overview_session = GetOverviewSession();
  RemoveSnappingWindowFromOverviewIfApplicable(overview_session, window);

  const float old_divider_position = divider_position_;
  // Get the divider position given by `snap_ratio` if exists, or if there is
  // pre-set `divider_position_`, use it, which can happen during tablet <->
  // clamshell transition or multi-user transition. If neither `snap_ratio` nor
  // `divider_position_` exists, calculate the divider position with the default
  // snap ratio i.e. `chromeos::kDefaultSnapRatio`.
  // TODO(michelefan): See if it is a valid case to not having `snap_ratio`
  // while `divider_position` is less than 0.
  if (absl::optional<float> snap_ratio = WindowState::Get(window)->snap_ratio();
      snap_ratio) {
    divider_position_ = GetDividerPosition(snap_position, *snap_ratio);
  } else if (divider_position_ < 0) {
    divider_position_ =
        GetDividerPosition(snap_position, chromeos::kDefaultSnapRatio);
  }

  if (state_ == State::kNoSnap) {
    Shell* shell = Shell::Get();
    // Add observers when the split view mode starts.
    shell->AddShellObserver(this);
    OverviewController::Get()->AddObserver(this);
    if (features::IsAdjustSplitViewForVKEnabled()) {
      keyboard::KeyboardUIController::Get()->AddObserver(this);
      shell->activation_client()->AddObserver(this);
    }

    if (!window_util::IsFasterSplitScreenOrSnapGroupEnabledInClamshell()) {
      // AutoSnapController will end overview in clamshell split view if a
      // window is not in transitional state. See
      // `AutoSnapController::AutoSnapWindowIfNeeded()`.
      // TODO(b/302397864): Handle this logic in
      // `OverviewSession::OnWindowActivating()`.
      auto_snap_controller_ =
          std::make_unique<AutoSnapController>(root_window_);
    }

    if (!IsInTabletMode() && IsInOverviewSession()) {
      if (auto* root_window_controller =
              RootWindowController::ForWindow(window)) {
        // Start the clamshell split overview session. It is too late to create
        // this in `OnOverviewModeStarting()`, since overview will already have
        // started and we are dragging `window` into split view.
        // TODO(b/294580642): Move this to SnapGroupController.
        root_window_controller->StartSplitViewOverviewSession(
            window, /*action=*/std::nullopt,
            /*type=*/std::nullopt, snap_action_source);
      }
    }

    default_snap_position_ = snap_position;

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

  if (split_view_type_ == SplitViewType::kTabletType && !split_view_divider_) {
    // `split_view_divider_` must be created after we start observing windows.
    split_view_divider_ =
        std::make_unique<SplitViewDivider>(this, divider_position_);
  }

  // This must be done before we push `divider_position_` to the closest fixed
  // ratio, since the minimum size will be respected there.
  const int new_divider_position = GetClosestFixedDividerPosition();
  const bool total_size_exceeds_work_area =
      divider_position_ + kSplitviewDividerShortSideLength +
          GetMinimumWindowLength(other_window, IsLayoutHorizontal(window)) >
      GetDividerPositionUpperLimit();
  if (split_view_divider_ && other_window && total_size_exceeds_work_area) {
    // If `other_window` can't fit in the opposite position, set
    // `divider_snap_animation_` to Hide then Show, to give off the
    // impression of bouncing the divider back to `old_divider_position`.
    // Note the duration is 2 * `kBouncingAnimationOneWayDuration` to
    // bounce out then in.
    tablet_resize_mode_ = TabletResizeMode::kFast;
    divider_snap_animation_ = std::make_unique<DividerSnapAnimation>(
        this, new_divider_position, old_divider_position,
        2 * kBouncingAnimationOneWayDuration, gfx::Tween::FAST_OUT_SLOW_IN_3);
    divider_snap_animation_->Hide();
    divider_snap_animation_->Show();
  }

  if (split_view_divider_) {
    divider_position_ = new_divider_position;
    split_view_divider_->UpdateDividerBounds();
  }

  base::RecordAction(base::UserMetricsAction("SplitView_SnapWindow"));
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
  gfx::Rect bounds = GetSnappedWindowBoundsInScreen(
      snap_position, window_for_minimum_size, snap_ratio);
  wm::ConvertRectFromScreen(root_window_, &bounds);
  return bounds;
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInParent(
    SnapPosition snap_position,
    aura::Window* window_for_minimum_size) {
  return GetSnappedWindowBoundsInParent(snap_position, window_for_minimum_size,
                                        chromeos::kDefaultSnapRatio);
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInScreen(
    SnapPosition snap_position,
    aura::Window* window_for_minimum_size,
    float snap_ratio) {
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  if (snap_position == SnapPosition::kNone) {
    return work_area_bounds_in_screen;
  }

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
  const bool in_tablet_mode = IsInTabletMode();
  const int work_area_size = GetDividerPositionUpperLimit();
  int divider_position = divider_position_ < 0
                             ? GetDividerPosition(snap_position, snap_ratio)
                             : divider_position_;

  int window_size;
  if (snap_left_or_top) {
    window_size = divider_position;
  } else {
    window_size = work_area_size - divider_position;
    // In tablet mode or when `IsSnapGroupEnabledInClamshellMode()` is true,
    // there is a divider widget of which `divider_position` refers to the left
    // or top, and so we should subtract the thickness.
    if (in_tablet_mode ||
        (split_view_divider_ && IsSnapGroupEnabledInClamshellMode())) {
      window_size -= kSplitviewDividerShortSideLength;
    }
  }

  const int minimum =
      GetMinimumWindowLength(window_for_minimum_size, horizontal);
  DCHECK(window_for_minimum_size || minimum == 0);
  if (window_size < minimum) {
    if (in_tablet_mode && !IsResizingWithDivider()) {
      // If window with `window_for_minimum_size` gets snapped, the
      // `split_view_divider_` will then be adjusted to its default position and
      // `window_size` will be computed accordingly.
      window_size = work_area_size / 2 - kSplitviewDividerShortSideLength / 2;
      // If `work_area_size` is odd, then the default divider position is
      // rounded down, toward the left or top, but then if `snap_left_or_top` is
      // false, that means `window_size` should now be rounded up.
      if (!snap_left_or_top && work_area_size % 2 == 1)
        ++window_size;
    } else {
      window_size = minimum;
    }
  }

  if (window_for_minimum_size && !in_tablet_mode) {
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

  // Get the parameter values for which `gfx::Rect::SetByBounds` would recreate
  // `work_area_bounds_in_screen`.
  int left = work_area_bounds_in_screen.x();
  int top = work_area_bounds_in_screen.y();
  int right = work_area_bounds_in_screen.right();
  int bottom = work_area_bounds_in_screen.bottom();

  // Make `snapped_window_bounds_in_screen` by modifying one of the above four
  // values: the one that represents the inner edge of the snapped bounds.
  int& left_or_top = horizontal ? left : top;
  int& right_or_bottom = horizontal ? right : bottom;
  if (snap_left_or_top) {
    right_or_bottom = left_or_top + window_size;
  } else {
    left_or_top = right_or_bottom - window_size;
  }

  gfx::Rect snapped_window_bounds_in_screen;
  snapped_window_bounds_in_screen.SetByBounds(left, top, right, bottom);
  return snapped_window_bounds_in_screen;
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInScreen(
    SnapPosition snap_position,
    aura::Window* window_for_minimum_size) {
  return GetSnappedWindowBoundsInScreen(snap_position, window_for_minimum_size,
                                        chromeos::kDefaultSnapRatio);
}

bool SplitViewController::ShouldUseWindowBoundsDuringFastResize() {
  return IsResizingWithDivider() &&
         tablet_resize_mode_ == TabletResizeMode::kFast;
}

int SplitViewController::GetDefaultDividerPosition() const {
  return GetDividerPosition(SnapPosition::kPrimary,
                            chromeos::kDefaultSnapRatio);
}

int SplitViewController::GetDividerPosition(SnapPosition snap_position,
                                            float snap_ratio) const {
  int divider_upper_limit = GetDividerPositionUpperLimit();
  // `snap_width` needs to be a float so that the rounding is performed at the
  // end of the computation of `next_divider_position`. It's important because a
  // 1-DIP gap between snapped windows precludes multiresizing. See b/262011280.
  const float snap_width = divider_upper_limit * snap_ratio;
  int next_divider_position = snap_position == SnapPosition::kPrimary
                                  ? snap_width
                                  : divider_upper_limit - snap_width;
  if (split_view_divider_ || split_view_type_ == SplitViewType::kTabletType) {
    // The divider may be visible in tablet mode, or between two windows in a
    // snap group in clamshell mode.
    // In tablet mode, we always consider the divider width even if
    // `split_view_divider_` is not initialized yet because
    // `ClientControlledState` may need to know the snapped bounds before
    // actually snapping the windows.
    next_divider_position -= kSplitviewDividerShortSideLength / 2;
  }
  return next_divider_position;
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
  shell->overview_controller()->RemoveObserver(this);
  if (features::IsAdjustSplitViewForVKEnabled()) {
    keyboard::KeyboardUIController::Get()->RemoveObserver(this);
    shell->activation_client()->RemoveObserver(this);
  }

  auto_snap_controller_.reset();

  if (end_reason != EndReason::kRootWindowDestroyed) {
    // `EndSplitView()` is also called upon `~RootWindowController()` and
    // `~SplitViewController()`, during which `root_window_` would have been
    // destroyed.
    RootWindowController::ForWindow(root_window_)
        ->EndSplitViewOverviewSession(
            SplitViewOverviewSessionExitPoint::kShutdown);
  }

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

  if (split_view_divider_) {
    split_view_divider_->OnWindowDragStarted(dragged_window);
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
         window == (IsLayoutPrimary(window) ? secondary_window_.get()
                                            : primary_window_.get());
}

void SplitViewController::AddObserver(SplitViewObserver* observer) {
  observers_.AddObserver(observer);
}

void SplitViewController::RemoveObserver(SplitViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SplitViewController::MaybeDetachWindow(aura::Window* dragged_window) {
  // If one of the windows in the snap group is dragged,  it may result in
  // ending split view and some post processings to be done such as removing
  // snap group and split view divider.
  auto* snap_group_controller = SnapGroupController::Get();
  if (snap_group_controller && InSplitViewMode() && !IsResizingWithDivider() &&
      !Shell::Get()->overview_controller()->InOverviewSession()) {
    OnSnappedWindowDetached(dragged_window,
                            WindowDetachedReason::kWindowDragged);
  }
}

void SplitViewController::OpenPartialOverviewToUpdateSnapGroup(
    SnapPosition snap_position) {
  // Update the value of `default_snap_position_` so that the other snap
  // position will not been observed in `OnOverviewModeStarting()`.
  default_snap_position_ = snap_position;

  if (!IsInOverviewSession() && !DesksController::Get()->animation()) {
    SnapGroupController::Get()->RemoveSnapGroupContainingWindow(
        primary_window_);
    split_view_divider_.reset();
    aura::Window* window = snap_position == SnapPosition::kPrimary
                               ? primary_window_
                               : secondary_window_;
    RootWindowController::ForWindow(window)->StartSplitViewOverviewSession(
        window, OverviewStartAction::kFasterSplitScreenSetup,
        OverviewEnterExitType::kNormal,
        WindowSnapActionSource::kSnapGroupWindowUpdate);
  }
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

void SplitViewController::OnWindowDestroyed(aura::Window* window) {
  DCHECK(InSplitViewMode());
  DCHECK(IsWindowInSplitView(window));
  auto iter = snapping_window_transformed_bounds_map_.find(window);
  if (iter != snapping_window_transformed_bounds_map_.end()) {
    snapping_window_transformed_bounds_map_.erase(iter);
  }

  OnSnappedWindowDetached(window, WindowDetachedReason::kWindowDestroyed);

  if (to_be_activated_window_ == window) {
    to_be_activated_window_ = nullptr;
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

    OnWindowSnapped(window, old_type, WindowSnapActionSource::kNotSpecified);
    if (do_divider_spawn_animation) {
      DoSplitDividerSpawnAnimation(window);
    }
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
  if (split_view_type_ == SplitViewType::kClamshellType)
    return;

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

  for (const auto& overview_item : current_grid->window_list()) {
    for (aura::Window* window : overview_item->GetWindows()) {
      CHECK(window);

      if (window == GetDefaultSnappedWindow()) {
        continue;
      }

      std::optional<float> snap_ratio = ComputeSnapRatio(window);
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
  if (split_view_type_ == SplitViewType::kClamshellType &&
      !IsSnapGroupEnabledInClamshellMode()) {
    EndSplitView();
  }
}

void SplitViewController::OnDisplayRemoved(
    const display::Display& old_display) {
  // If the `root_window_`is the root window of the display which is going to
  // be removed, there's no need to start overview.
  if (GetRootWindowSettings(root_window_)->display_id ==
      display::kInvalidDisplayId) {
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
  // Postpone `EndSplitViewAfterResizingAtEdgeIfAppropriate()` until after the
  // adjustment, because the new display metrics will be used to compare the
  // divider position against the edges of the screen.
  if (IsDividerAnimating()) {
    StopAndShoveAnimatedDivider();
    EndResizeWithDividerImpl();
  }

  if ((metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION) ||
      (metrics & display::DisplayObserver::DISPLAY_METRIC_WORK_AREA)) {
    // Set default `divider_closest_ratio_` to kFixedPositionRatios[1].
    if (std::isnan(divider_closest_ratio_))
      divider_closest_ratio_ = kFixedPositionRatios[1];

    // Reverse the position ratio if top/left window changes.
    if (is_previous_layout_right_side_up != IsLayoutPrimary(display))
      divider_closest_ratio_ = 1.f - divider_closest_ratio_;
    divider_position_ = static_cast<int>(divider_closest_ratio_ *
                                         GetDividerPositionUpperLimit()) -
                        kSplitviewDividerShortSideLength / 2;
  }

  // For other display configuration changes, we only move the divider to the
  // closest fixed position.
  if (!IsResizingWithDivider()) {
    divider_position_ = GetClosestFixedDividerPosition();
  }

  EndSplitViewAfterResizingAtEdgeIfAppropriate();
  if (!InSplitViewMode())
    return;

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
      OnTabletModeStarting();
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
  if (!features::IsAdjustSplitViewForVKEnabled()) {
    return;
  }

  // If the bottom window is moved for the virtual keyboard (the split view
  // divider bar is unadjustable), when the bottom window lost active, restore
  // to the original layout.
  if (!split_view_divider_ || split_view_divider_->IsAdjustable()) {
    return;
  }

  if (IsLayoutHorizontal(root_window_)) {
    return;
  }

  aura::Window* bottom_window = GetPhysicalRightOrBottomWindow();
  if (!bottom_window)
    return;

  if (bottom_window->Contains(lost_active) &&
      !bottom_window->Contains(gained_active)) {
    UpdateSnappedWindowsAndDividerBounds();
  }
}

void SplitViewController::OnSnapGroupCreated() {
  CHECK(IsSnapGroupEnabledInClamshellMode());
  CreateSplitViewDividerInClamshell();
}

void SplitViewController::OnSnapGroupRemoved() {
  CHECK(Shell::Get()->snap_group_controller());
  split_view_divider_.reset();
}

void SplitViewController::StartResizeWithDivider(
    const gfx::Point& location_in_screen) {
  StartTabletResize();
}

void SplitViewController::UpdateResizeWithDivider(
    const gfx::Point& location_in_screen) {
  // This updates `tablet_resize_mode_` based on drag speed.
  UpdateTabletResizeMode(base::TimeTicks::Now(), location_in_screen);

  // Update `divider_position_`.
  UpdateDividerPosition(location_in_screen);
  NotifyDividerPositionChanged();
  UpdateSnappedWindowsAndDividerBounds();

  // Update the resize backdrop, as well as the black scrim layer's bounds and
  // opacity.
  // TODO(b/298515546): Add performant resizing pattern.
  UpdateResizeBackdrop();
  UpdateBlackScrim(location_in_screen);

  // Apply window transform if necessary.
  SetWindowsTransformDuringResizing();
}

void SplitViewController::EndResizeWithDivider(
    const gfx::Point& location_in_screen) {
  UpdateDividerPosition(location_in_screen);
  NotifyDividerPositionChanged();

  // Need to update snapped windows bounds even if the split view mode may have
  // to exit. Otherwise it's possible for a snapped window stuck in the edge of
  // of the screen while overview mode is active.
  UpdateSnappedWindowsAndDividerBounds();
  NotifyWindowResized();

  EndTabletResize();
}

aura::Window::Windows SplitViewController::GetLayoutWindows() const {
  aura::Window::Windows window_list;
  for (aura::Window* window : {primary_window_, secondary_window_}) {
    if (window) {
      window_list.push_back(window);
    }
  }
  return window_list;
}

aura::Window* SplitViewController::GetPhysicalLeftOrTopWindow() {
  DCHECK(root_window_);
  return IsLayoutPrimary(root_window_) ? primary_window_.get()
                                       : secondary_window_.get();
}

aura::Window* SplitViewController::GetPhysicalRightOrBottomWindow() {
  DCHECK(root_window_);
  return IsLayoutPrimary(root_window_) ? secondary_window_.get()
                                       : primary_window_.get();
}

void SplitViewController::StartObserving(aura::Window* window) {
  if (window && !window->HasObserver(this)) {
    Shell::Get()->shadow_controller()->UpdateShadowForWindow(window);
    window->AddObserver(this);
    WindowState::Get(window)->AddObserver(this);
    if (split_view_divider_) {
      split_view_divider_->AddObservedWindow(window);
    }
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
  DCHECK(previous_state != State::kNoSnap || state_ != State::kNoSnap);
  for (auto& observer : observers_) {
    observer.OnSplitViewStateChanged(previous_state, state_);
  }
}

void SplitViewController::NotifyDividerPositionChanged() {
  if (!InSplitViewMode()) {
    return;
  }
  for (auto& observer : observers_)
    observer.OnSplitViewDividerPositionChanged();
}

void SplitViewController::NotifyWindowResized() {
  if (!InSplitViewMode()) {
    return;
  }
  for (auto& observer : observers_)
    observer.OnSplitViewWindowResized();
}

void SplitViewController::NotifyWindowSwapped() {
  for (auto& observer : observers_)
    observer.OnSplitViewWindowSwapped();
}

void SplitViewController::UpdateDividerPositionOnWindowResize(
    aura::Window* window,
    const gfx::Rect& new_bounds) {
  CHECK_EQ(root_window_, window->GetRootWindow());
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

void SplitViewController::MaybeEndOverviewOnWindowResize(aura::Window* window) {
  const int divider_upper_limit(GetDividerPositionUpperLimit());
  if (divider_position_ < divider_upper_limit * chromeos::kOneThirdSnapRatio ||
      divider_position_ > divider_upper_limit * chromeos::kTwoThirdSnapRatio) {
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEndAction::kSplitView);
    WindowState::Get(window)->Maximize();
  }
}

void SplitViewController::CreateSplitViewDividerInClamshell() {
  CHECK(InClamshellSplitViewMode());
  divider_position_ = GetClosestFixedDividerPosition();
  split_view_divider_ =
      std::make_unique<SplitViewDivider>(this, divider_position_);
  UpdateSnappedWindowsAndDividerBounds();
  // No need to notify observers, since the divider is only created between two
  // windows.
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

void SplitViewController::UpdateSnappedBounds(aura::Window* window) {
  DCHECK(IsWindowInSplitView(window));
  WindowState* window_state = WindowState::Get(window);
  // SplitViewController will use the divider position to determine the
  // window's snapped bounds.
  const bool in_tablet_mode = IsInTabletMode();
  // TODO(b/264962634): Remove this workaround. Probably, we can rewrite
  // `TabletModeWindowState::UpdateWindowPosition` to include this logic.
  if (window->GetProperty(aura::client::kAppType) ==
      static_cast<int>(AppType::ARC_APP)) {
    const gfx::Rect requested_bounds =
        in_tablet_mode
            ? TabletModeWindowState::GetBoundsInTabletMode(window_state)
            : GetSnappedWindowBoundsInScreen(GetPositionOfSnappedWindow(window),
                                             window);
    const SetBoundsWMEvent event(requested_bounds,
                                 /*animate=*/true);
    window_state->OnWMEvent(&event);
    return;
  }

  if (in_tablet_mode) {
    TabletModeWindowState::UpdateWindowPosition(
        window_state, WindowState::BoundsChangeAnimationType::kAnimate);
    return;
  } else if (IsSnapGroupEnabledInClamshellMode()) {
    const gfx::Rect requested_bounds = GetSnappedWindowBoundsInScreen(
        GetPositionOfSnappedWindow(window), window);
    const SetBoundsWMEvent event(requested_bounds, /*animate=*/true);
    window_state->OnWMEvent(&event);
    return;
  }

  window_state->UpdateSnappedBounds();
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
  int divider_upper_limit = GetDividerPositionUpperLimit();
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

void SplitViewController::UpdateDividerPosition(
    const gfx::Point& location_in_screen) {
  CHECK(split_view_divider_);
  if (IsLayoutHorizontal(root_window_)) {
    divider_position_ += location_in_screen.x() -
                         split_view_divider_->previous_event_location_.x();
  } else {
    divider_position_ += location_in_screen.y() -
                         split_view_divider_->previous_event_location_.y();
  }

  divider_position_ = std::max(0, divider_position_);
}

int SplitViewController::GetClosestFixedDividerPosition() {
  // The values in |kFixedPositionRatios| represent the fixed position of the
  // center of the divider while |divider_position_| represent the origin of the
  // divider rectangle. So, before calling FindClosestFixedPositionRatio,
  // extract the center from |divider_position_|. The result will also be the
  // center of the divider, so extract the origin, unless the result is on of
  // the endpoints.
  int divider_upper_limit = GetDividerPositionUpperLimit();
  divider_closest_ratio_ = FindClosestPositionRatio(
      float(divider_position_ + kSplitviewDividerShortSideLength / 2) /
      divider_upper_limit);
  int fixed_position = divider_upper_limit * divider_closest_ratio_;
  if (divider_closest_ratio_ > 0.f && divider_closest_ratio_ < 1.f) {
    fixed_position -= kSplitviewDividerShortSideLength / 2;
  }
  return fixed_position;
}

void SplitViewController::StopAndShoveAnimatedDivider() {
  CHECK(IsDividerAnimating());

  StopSnapAnimation();
  NotifyDividerPositionChanged();
  UpdateSnappedWindowsAndDividerBounds();
}

void SplitViewController::StopSnapAnimation() {
  divider_snap_animation_->Stop();
  divider_position_ = divider_snap_animation_->ending_position();
}

bool SplitViewController::ShouldEndSplitViewAfterResizingAtEdge() {
  DCHECK(InTabletSplitViewMode() || IsSnapGroupEnabledInClamshellMode());

  return divider_position_ == 0 ||
         divider_position_ == GetDividerPositionUpperLimit();
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

  return divider_position_ == 0 ? GetPhysicalRightOrBottomWindow()
                                : GetPhysicalLeftOrTopWindow();
}

int SplitViewController::GetDividerPositionUpperLimit() const {
  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);
  return IsLayoutHorizontal(root_window_) ? work_area_bounds.width()
                                          : work_area_bounds.height();
}

void SplitViewController::OnWindowSnapped(
    aura::Window* window,
    std::optional<chromeos::WindowStateType> previous_state,
    WindowSnapActionSource snap_action_source) {
  RestoreTransformIfApplicable(window);
  UpdateStateAndNotifyObservers();

  if (state_ == State::kBothSnapped && IsSnapGroupEnabledInClamshellMode()) {
    SnapGroupController* snap_group_controller = SnapGroupController::Get();
    // TODO(b/286963080): Move this to SnapGroupController.
    if (!snap_group_controller->AreWindowsInSnapGroup(primary_window_,
                                                      secondary_window_)) {
      snap_group_controller->AddSnapGroup(primary_window_, secondary_window_);
    }

    if (!split_view_divider_) {
      CreateSplitViewDividerInClamshell();
      split_view_divider_->RefreshStackingOrder();
    }
  }

  // If the snapped window was removed from overview and was the active window
  // before entering overview, it should be the active window after snapping in
  // splitview.
  if (to_be_activated_window_ == window) {
    to_be_activated_window_ = nullptr;
    wm::ActivateWindow(window);
  }

  // In tablet mode, if the window was previously floated, the other side is
  // available, and there is another non-minimized window, do not enter overview
  // but instead snap that window to the opposite side.
  if (previous_state &&
      *previous_state == chromeos::WindowStateType::kFloated &&
      IsInTabletMode() && !BothSnapped()) {
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

  // Overview will be opened on the other side of the screen if there is
  // only one snapped window in split screen when in tablet mode when
  // `WillStartOverview()` returns true, the check will happen in
  // `OverviewController`.
  if (WillStartOverview()) {
    RootWindowController::ForWindow(window)->StartSplitViewOverviewSession(
        window, overview_start_action_, enter_exit_overview_type_,
        snap_action_source);
    overview_start_action_.reset();
    enter_exit_overview_type_.reset();
    return;
  }

  // When `window` is snapped, it may push the divider position due to its
  // minimum size constraint. If the other snap position holds a previously
  // snapped window, we should update the other's snapped bounds to account for
  // the newly snapped `window`.
  if (primary_window_ && secondary_window_) {
    UpdateSnappedBounds(window == primary_window_ ? secondary_window_.get()
                                                  : primary_window_.get());
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
  if (is_window_destroyed) {
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

  if (!is_window_destroyed) {
    StopObserving(position_of_snapped_window);
  }

  // End the Split View mode for the following two cases:
  // 1. If there is no snapped window at this moment. Note that this will update
  // overview window grid bounds if the overview mode is active at the moment;
  // 2.  When `kSnapGroup` is enabled and feature param
  // `kAutomaticallyLockGroup` is true, `SplitViewController` will no longer
  // manage the window(s) on one window detached.
  if ((!primary_window_ && !secondary_window_) ||
      (IsSnapGroupEnabledInClamshellMode() &&
       (!primary_window_ || !secondary_window_))) {
    EndSplitView(reason == WindowDetachedReason::kWindowDragged
                     ? EndReason::kWindowDragStarted
                     : EndReason::kNormal);

    // TODO(crbug.com/1351562): Consider not allowing one snapped window to be
    // floated. Then this should be a DCHECK.
  } else {
    DCHECK(InTabletSplitViewMode());
    aura::Window* other_window =
        GetSnappedWindow(position_of_snapped_window == SnapPosition::kPrimary
                             ? SnapPosition::kSecondary
                             : SnapPosition::kPrimary);

    if (reason == WindowDetachedReason::kWindowFloated && IsInTabletMode()) {
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
    std::vector<float>* out_position_ratios) {
  const bool landscape = IsCurrentScreenOrientationLandscape();
  const int min_left_size =
      GetMinimumWindowLength(GetPhysicalLeftOrTopWindow(), landscape);
  const int min_right_size =
      GetMinimumWindowLength(GetPhysicalRightOrBottomWindow(), landscape);
  const int divider_upper_limit = GetDividerPositionUpperLimit();
  const float min_size_left_ratio =
      static_cast<float>(min_left_size) / divider_upper_limit;
  const float min_size_right_ratio =
      static_cast<float>(min_right_size) / divider_upper_limit;
  if (min_size_left_ratio > chromeos::kOneThirdSnapRatio) {
    // If `primary_window_` can't fit in 1/3, remove 0.33f divider position.
    base::Erase(*out_position_ratios, chromeos::kOneThirdSnapRatio);
  }
  if (min_size_right_ratio > chromeos::kOneThirdSnapRatio) {
    // If `secondary_window_` can't fit in 1/3, remove 0.67f divider position.
    base::Erase(*out_position_ratios, chromeos::kTwoThirdSnapRatio);
  }
  // Remove 0.5f if a window cannot be snapped. We can get into this state by
  // snapping a window to two thirds.
  if (min_size_left_ratio > chromeos::kDefaultSnapRatio ||
      min_size_right_ratio > chromeos::kDefaultSnapRatio) {
    base::Erase(*out_position_ratios, chromeos::kDefaultSnapRatio);
  }
}

float SplitViewController::FindClosestPositionRatio(float current_ratio) {
  float closest_ratio = 0.f;
  std::vector<float> position_ratios(
      kFixedPositionRatios,
      kFixedPositionRatios + std::size(kFixedPositionRatios));
  ModifyPositionRatios(&position_ratios);
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

void SplitViewController::SetWindowsTransformDuringResizing() {
  CHECK(InTabletSplitViewMode() || IsSnapGroupEnabledInClamshellMode());
  CHECK_GE(divider_position_, 0);
  aura::Window* left_or_top_window = GetPhysicalLeftOrTopWindow();
  aura::Window* right_or_bottom_window = GetPhysicalRightOrBottomWindow();
  if (left_or_top_window) {
    SetWindowTransformDuringResizing(left_or_top_window, divider_position_);
  }
  if (right_or_bottom_window) {
    SetWindowTransformDuringResizing(right_or_bottom_window, divider_position_);
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
    if (window_state->is_dragged()) {
      CHECK(split_view_divider_);
      window_state->OnCompleteDrag(gfx::PointF(GetEndDragLocationInScreen(
          window, split_view_divider_->previous_event_location_)));
      window_state->DeleteDragDetails();
    }
  }
}

void SplitViewController::StartTabletResize() {
  base::RecordAction(base::UserMetricsAction("SplitView_ResizeWindows"));
  if (state_ == State::kBothSnapped) {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        split_view_divider_->divider_widget()->GetCompositor(),
        kTabletSplitViewResizeMultiHistogram,
        kTabletSplitViewResizeMultiMaxLatencyHistogram);
    return;
  }

  CHECK(GetOverviewSession());
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
  accumulated_drag_time_ticks_ = base::TimeTicks::Now();
  accumulated_drag_distance_ = 0;

  tablet_resize_mode_ = TabletResizeMode::kNormal;
}

void SplitViewController::EndTabletResize() {
  presentation_time_recorder_.reset();

  // TODO(xdai): Use fade out animation instead of just removing it.
  black_scrim_layer_.reset();

  resize_timer_.Stop();
  tablet_resize_mode_ = TabletResizeMode::kNormal;

  const int target_divider_position = GetClosestFixedDividerPosition();
  // TODO(b/298515283): Separate Snap Group and tablet resize.
  if (divider_position_ == target_divider_position ||
      IsSnapGroupEnabledInClamshellMode()) {
    EndResizeWithDividerImpl();
    EndSplitViewAfterResizingAtEdgeIfAppropriate();
  } else {
    divider_snap_animation_ = std::make_unique<DividerSnapAnimation>(
        this, divider_position_, target_divider_position,
        base::Milliseconds(300), gfx::Tween::EASE_IN);
    divider_snap_animation_->Show();
  }
}

void SplitViewController::EndTabletResizeImpl() {
  // The backdrop layers are removed here (rather than in
  // `EndResizeWithDivider()`) since they may be used while the divider is
  // animating to a snapped position.
  left_resize_backdrop_layer_.reset();
  right_resize_backdrop_layer_.reset();

  // Resize may not end with `EndResizeWithDivider()`, so make sure to clear
  // here too.
  resize_timer_.Stop();
}

void SplitViewController::EndResizeWithDividerImpl() {
  DCHECK(InSplitViewMode());
  if (split_view_divider_) {
    // TODO(b/315854755): Move `DividerSnapAnimation` to `SplitViewDivider` and
    // see if we can remove this.
    split_view_divider_->set_is_resizing_with_divider(false);
  }

  EndTabletResizeImpl();
  presentation_time_recorder_.reset();
  RestoreWindowsTransformAfterResizing();
  FinishWindowResizing(primary_window_);
  FinishWindowResizing(secondary_window_);
}

void SplitViewController::OnResizeTimer() {
  if (InSplitViewMode() && split_view_divider_) {
    split_view_divider_->ResizeWithDivider(
        split_view_divider_->previous_event_location_);
  }
}

void SplitViewController::UpdateTabletResizeMode(
    base::TimeTicks event_time_ticks,
    const gfx::Point& event_location) {
  CHECK(presentation_time_recorder_);
  presentation_time_recorder_->RequestNext();

  if (IsLayoutHorizontal(root_window_)) {
    accumulated_drag_distance_ += std::abs(
        event_location.x() - split_view_divider_->previous_event_location_.x());
  } else {
    accumulated_drag_distance_ += std::abs(
        event_location.y() - split_view_divider_->previous_event_location_.y());
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

void SplitViewController::OnTabletModeStarting() {
  split_view_type_ = SplitViewType::kTabletType;
}

void SplitViewController::OnTabletModeStarted() {
  is_previous_layout_right_side_up_ = IsCurrentScreenOrientationPrimary();
  // If splitview is active when tablet mode is starting, create the split view
  // divider if not exists and adjust the `divider_position_` to be one
  // of the fixed positions.
  if (InSplitViewMode()) {
    divider_position_ = GetClosestFixedDividerPosition();
    if (!split_view_divider_) {
      split_view_divider_ =
          std::make_unique<SplitViewDivider>(this, divider_position_);
    }

    UpdateSnappedWindowsAndDividerBounds();
    NotifyDividerPositionChanged();
  }
}

void SplitViewController::OnTabletModeEnding() {
  split_view_type_ = SplitViewType::kClamshellType;

  // `OnTabletModeEnding()` can also be called during test teardown.
  const bool is_divider_animating = IsDividerAnimating();
  if (IsResizingWithDivider() || is_divider_animating) {
    if (is_divider_animating) {
      StopAndShoveAnimatedDivider();
    }

    EndResizeWithDividerImpl();
  }

  // There is no divider in clamshell split view unless the two snapped windows
  // belong to a snap group.
  auto* snap_group_controller = SnapGroupController::Get();
  if (state_ != State::kBothSnapped || !snap_group_controller ||
      !snap_group_controller->AreWindowsInSnapGroup(primary_window_,
                                                    secondary_window_)) {
    split_view_divider_.reset();
  }
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
  if (split_view_divider_)
    split_view_divider_->OnWindowDragEnded();

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

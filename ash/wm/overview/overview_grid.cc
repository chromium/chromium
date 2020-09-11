// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_grid.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/metrics/histogram_macros.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/cleanup_animation_observer.h"
#include "ash/wm/overview/drop_target_view.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_delegate.h"
#include "ash/wm/overview/overview_grid_event_handler.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/bind.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/numerics/ranges.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

// Windows are not allowed to get taller than this.
constexpr int kMaxHeight = 512;

// Margins reserved in the overview mode.
constexpr float kOverviewInsetRatio = 0.05f;

// Additional vertical inset reserved for windows in overview mode.
constexpr float kOverviewVerticalInset = 0.1f;

// Number of rows for windows in tablet overview mode.
constexpr int kTabletLayoutRow = 2;

constexpr int kMinimumItemsForNewLayout = 6;

// Wait a while before unpausing the occlusion tracker after a scroll has
// completed as the user may start another scroll.
constexpr base::TimeDelta kOcclusionUnpauseDurationForScroll =
    base::TimeDelta::FromMilliseconds(500);

constexpr base::TimeDelta kOcclusionUnpauseDurationForRotation =
    base::TimeDelta::FromMilliseconds(300);

// Histogram names for overview enter/exit smoothness in clamshell,
// tablet mode and splitview.
constexpr char kOverviewEnterClamshellHistogram[] =
    "Ash.Overview.AnimationSmoothness.Enter.ClamshellMode";
constexpr char kOverviewEnterSingleClamshellHistogram[] =
    "Ash.Overview.AnimationSmoothness.Enter.SingleClamshellMode";
constexpr char kOverviewEnterTabletHistogram[] =
    "Ash.Overview.AnimationSmoothness.Enter.TabletMode";
constexpr char kOverviewEnterMinimizedTabletHistogram[] =
    "Ash.Overview.AnimationSmoothness.Enter.MinimizedTabletMode";
constexpr char kOverviewEnterSplitViewHistogram[] =
    "Ash.Overview.AnimationSmoothness.Enter.SplitView";

constexpr char kOverviewExitClamshellHistogram[] =
    "Ash.Overview.AnimationSmoothness.Exit.ClamshellMode";
constexpr char kOverviewExitSingleClamshellHistogram[] =
    "Ash.Overview.AnimationSmoothness.Exit.SingleClamshellMode";
constexpr char kOverviewExitTabletHistogram[] =
    "Ash.Overview.AnimationSmoothness.Exit.TabletMode";
constexpr char kOverviewExitMinimizedTabletHistogram[] =
    "Ash.Overview.AnimationSmoothness.Exit.MinimizedTabletMode";
constexpr char kOverviewExitSplitViewHistogram[] =
    "Ash.Overview.AnimationSmoothness.Exit.SplitView";

// The UMA histogram that records presentation time for grid scrolling in the
// new overview layout.
constexpr char kOverviewScrollHistogram[] =
    "Ash.Overview.Scroll.PresentationTime.TabletMode";
constexpr char kOverviewScrollMaxLatencyHistogram[] =
    "Ash.Overview.Scroll.PresentationTime.MaxLatency.TabletMode";

template <const char* clamshell_single_name,
          const char* clamshell_multi_name,
          const char* tablet_name,
          const char* splitview_name,
          const char* tablet_minimized_name>
class OverviewMetricsTracker : public OverviewGrid::MetricsTracker {
 public:
  OverviewMetricsTracker(ui::Compositor* compositor,
                         bool in_split_view,
                         bool single_animation_in_clamshell,
                         bool minimized_in_tablet)
      : tracker_(compositor->RequestNewThroughputTracker()) {
    tracker_.Start(metrics_util::ForSmoothness(base::BindRepeating(
        &OverviewMetricsTracker::ReportOverviewSmoothness, in_split_view,
        single_animation_in_clamshell, minimized_in_tablet)));
  }
  OverviewMetricsTracker(const OverviewMetricsTracker&) = delete;
  OverviewMetricsTracker& operator=(const OverviewMetricsTracker&) = delete;
  ~OverviewMetricsTracker() override { tracker_.Stop(); }

  static void ReportOverviewSmoothness(bool in_split_view,
                                       bool single_animation_in_clamshell,
                                       bool minimized_in_tablet,
                                       int smoothness) {
    if (single_animation_in_clamshell)
      UMA_HISTOGRAM_PERCENTAGE_IN_CLAMSHELL(clamshell_single_name, smoothness);
    else
      UMA_HISTOGRAM_PERCENTAGE_IN_CLAMSHELL(clamshell_multi_name, smoothness);

    if (minimized_in_tablet) {
      UMA_HISTOGRAM_PERCENTAGE_IN_TABLET_NON_SPLITVIEW(
          in_split_view, tablet_minimized_name, smoothness);
    } else {
      UMA_HISTOGRAM_PERCENTAGE_IN_TABLET_NON_SPLITVIEW(in_split_view,
                                                       tablet_name, smoothness);
    }
    UMA_HISTOGRAM_PERCENTAGE_IN_SPLITVIEW(in_split_view, splitview_name,
                                          smoothness);
  }

 private:
  ui::ThroughputTracker tracker_;
};

using OverviewEnterMetricsTracker =
    OverviewMetricsTracker<kOverviewEnterSingleClamshellHistogram,
                           kOverviewEnterClamshellHistogram,
                           kOverviewEnterTabletHistogram,
                           kOverviewEnterSplitViewHistogram,
                           kOverviewEnterMinimizedTabletHistogram>;
using OverviewExitMetricsTracker =
    OverviewMetricsTracker<kOverviewExitSingleClamshellHistogram,
                           kOverviewExitClamshellHistogram,
                           kOverviewExitTabletHistogram,
                           kOverviewExitSplitViewHistogram,
                           kOverviewExitMinimizedTabletHistogram>;

class ShutdownAnimationMetricsTrackerObserver : public OverviewObserver {
 public:
  ShutdownAnimationMetricsTrackerObserver(ui::Compositor* compositor,
                                          bool in_split_view,
                                          bool single_animation,
                                          bool minimized_in_tablet)
      : metrics_tracker_(compositor,
                         in_split_view,
                         single_animation,
                         minimized_in_tablet) {
    Shell::Get()->overview_controller()->AddObserver(this);
  }
  ShutdownAnimationMetricsTrackerObserver(
      const ShutdownAnimationMetricsTrackerObserver&) = delete;
  ShutdownAnimationMetricsTrackerObserver& operator=(
      const ShutdownAnimationMetricsTrackerObserver&) = delete;
  ~ShutdownAnimationMetricsTrackerObserver() override {
    Shell::Get()->overview_controller()->RemoveObserver(this);
  }

  // OverviewObserver:
  void OnOverviewModeEndingAnimationComplete(bool canceled) override {
    delete this;
  }

 private:
  OverviewExitMetricsTracker metrics_tracker_;
};

// Creates |drop_target_widget_|. It's created when a window or overview item is
// dragged around, and destroyed when the drag ends.
std::unique_ptr<views::Widget> CreateDropTargetWidget(
    aura::Window* root_window,
    aura::Window* dragged_window) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.activatable = views::Widget::InitParams::Activatable::ACTIVATABLE_NO;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.name = "OverviewDropTarget";
  params.accept_events = false;
  params.parent = desks_util::GetActiveDeskContainerForRoot(root_window);
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  auto widget = std::make_unique<views::Widget>();
  widget->set_focus_on_creation(false);
  widget->Init(std::move(params));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);

  // Show plus icon if drag a tab from a multi-tab window.
  widget->SetContentsView(std::make_unique<DropTargetView>(
      dragged_window->GetProperty(ash::kTabDraggingSourceWindowKey)));
  aura::Window* drop_target_window = widget->GetNativeWindow();
  drop_target_window->parent()->StackChildAtBottom(drop_target_window);
  widget->Show();
  return widget;
}

float GetWantedDropTargetOpacity(
    SplitViewDragIndicators::WindowDraggingState window_dragging_state) {
  switch (window_dragging_state) {
    case SplitViewDragIndicators::WindowDraggingState::kNoDrag:
    case SplitViewDragIndicators::WindowDraggingState::kOtherDisplay:
    case SplitViewDragIndicators::WindowDraggingState::kToSnapLeft:
    case SplitViewDragIndicators::WindowDraggingState::kToSnapRight:
      return 0.f;
    case SplitViewDragIndicators::WindowDraggingState::kFromOverview:
    case SplitViewDragIndicators::WindowDraggingState::kFromTop:
    case SplitViewDragIndicators::WindowDraggingState::kFromShelf:
      return 1.f;
  }
}

gfx::Insets GetGridInsets(const gfx::Rect& grid_bounds) {
  const int horizontal_inset =
      base::ClampFloor(std::min(kOverviewInsetRatio * grid_bounds.width(),
                                kOverviewInsetRatio * grid_bounds.height()));
  const int vertical_inset =
      horizontal_inset +
      kOverviewVerticalInset * (grid_bounds.height() - 2 * horizontal_inset);

  return gfx::Insets(std::max(0, vertical_inset - kWindowMargin),
                     std::max(0, horizontal_inset - kWindowMargin));
}

bool ShouldExcludeItemFromGridLayout(
    OverviewItem* item,
    const base::flat_set<OverviewItem*>& ignored_items) {
  return item->animating_to_close() || ignored_items.contains(item);
}

}  // namespace

// The class to observe the overview window that the dragged tabs will merge
// into. After the dragged tabs merge into the overview window, and if the
// overview window represents a minimized window, we need to update the
// overview minimized widget's content view so that it reflects the merge.
class OverviewGrid::TargetWindowObserver : public aura::WindowObserver {
 public:
  TargetWindowObserver() = default;
  ~TargetWindowObserver() override { StopObserving(); }

  void StartObserving(aura::Window* window) {
    if (target_window_)
      StopObserving();

    target_window_ = window;
    target_window_->AddObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK_EQ(window, target_window_);
    // When the property is cleared, the dragged window should have been merged
    // into |target_window_|, update the corresponding window item in overview.
    if (key == kIsDeferredTabDraggingTargetWindowKey &&
        !window->GetProperty(kIsDeferredTabDraggingTargetWindowKey)) {
      UpdateWindowItemInOverviewContaining(window);
      StopObserving();
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window, target_window_);
    StopObserving();
  }

 private:
  void UpdateWindowItemInOverviewContaining(aura::Window* window) {
    OverviewController* overview_controller =
        Shell::Get()->overview_controller();
    if (!overview_controller->InOverviewSession())
      return;

    OverviewGrid* grid =
        overview_controller->overview_session()->GetGridWithRootWindow(
            window->GetRootWindow());
    if (!grid)
      return;

    OverviewItem* item = grid->GetOverviewItemContaining(window);
    if (!item)
      return;

    item->UpdateItemContentViewForMinimizedWindow();
  }

  void StopObserving() {
    if (target_window_)
      target_window_->RemoveObserver(this);
    target_window_ = nullptr;
  }

  aura::Window* target_window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TargetWindowObserver);
};

OverviewGrid::OverviewGrid(aura::Window* root_window,
                           const std::vector<aura::Window*>& windows,
                           OverviewSession* overview_session)
    : root_window_(root_window),
      overview_session_(overview_session),
      split_view_drag_indicators_(
          ShouldAllowSplitView()
              ? std::make_unique<SplitViewDragIndicators>(root_window)
              : nullptr),
      bounds_(GetGridBoundsInScreen(root_window)) {
  for (auto* window : windows) {
    if (window->GetRootWindow() != root_window)
      continue;

    // Stop ongoing animations before entering overview mode. Because we are
    // deferring SetTransform of the windows beneath the window covering the
    // available workspace, we need to set the correct transforms of these
    // windows before entering overview mode again in the
    // OnImplicitAnimationsCompleted() of the observer of the
    // available-workspace-covering window's animation.
    auto* animator = window->layer()->GetAnimator();
    if (animator->is_animating())
      window->layer()->GetAnimator()->StopAnimating();
    window_list_.push_back(
        std::make_unique<OverviewItem>(window, overview_session_, this));
  }
}

OverviewGrid::~OverviewGrid() = default;

void OverviewGrid::Shutdown() {
  EndNudge();

  SplitViewController::Get(root_window_)->RemoveObserver(this);
  ScreenRotationAnimator::GetForRootWindow(root_window_)->RemoveObserver(this);
  Shell::Get()->wallpaper_controller()->RemoveObserver(this);
  grid_event_handler_.reset();

  bool has_non_cover_animating = false;
  int animate_count = 0;

  for (const auto& window : window_list_) {
    if (window->should_animate_when_exiting() && !has_non_cover_animating) {
      has_non_cover_animating |=
          !CanCoverAvailableWorkspace(window->GetWindow());
      animate_count++;
    }
    window->Shutdown();
  }
  bool single_animation_in_clamshell =
      (animate_count == 1 && !has_non_cover_animating) &&
      !Shell::Get()->tablet_mode_controller()->InTabletMode();

  const bool in_split_view =
      SplitViewController::Get(root_window_)->InSplitViewMode();
  // OverviewGrid in splitscreen does not include the window to be activated.
  if (!window_list_.empty() || in_split_view) {
    bool minimized_in_tablet = overview_session_->enter_exit_overview_type() ==
                               OverviewEnterExitType::kFadeOutExit;
    // The following instance self-destructs when shutdown animation ends.
    new ShutdownAnimationMetricsTrackerObserver(
        root_window_->layer()->GetCompositor(), in_split_view,
        single_animation_in_clamshell, minimized_in_tablet);
  }

  window_list_.clear();
  overview_session_ = nullptr;
}

void OverviewGrid::PrepareForOverview() {
  if (!ShouldAnimateWallpaper(root_window_))
    MaybeInitDesksWidget();

  for (const auto& window : window_list_)
    window->PrepareForOverview();
  SplitViewController::Get(root_window_)->AddObserver(this);
  if (Shell::Get()->tablet_mode_controller()->InTabletMode())
    ScreenRotationAnimator::GetForRootWindow(root_window_)->AddObserver(this);

  grid_event_handler_ = std::make_unique<OverviewGridEventHandler>(this);
  Shell::Get()->wallpaper_controller()->AddObserver(this);
}

void OverviewGrid::PositionWindows(
    bool animate,
    const base::flat_set<OverviewItem*>& ignored_items,
    OverviewTransition transition) {
  if (!overview_session_ || suspend_reposition_ || window_list_.empty())
    return;

  DCHECK_NE(transition, OverviewTransition::kExit);

  std::vector<gfx::RectF> rects =
      ShouldUseTabletModeGridLayout() &&
              (window_list_.size() - ignored_items.size() >=
               kMinimumItemsForNewLayout)
          ? GetWindowRectsForTabletModeLayout(ignored_items)
          : GetWindowRects(ignored_items);

  if (transition == OverviewTransition::kEnter) {
    CalculateWindowListAnimationStates(/*selected_item=*/nullptr, transition,
                                       rects);
  }

  // Position the windows centering the left-aligned rows vertically. Do not
  // position items in |ignored_items|.
  OverviewAnimationType animation_type = OVERVIEW_ANIMATION_NONE;
  switch (transition) {
    case OverviewTransition::kEnter: {
      const bool entering_from_home =
          overview_session_->enter_exit_overview_type() ==
          OverviewEnterExitType::kFadeInEnter;
      animation_type = entering_from_home
                           ? OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER
                           : OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER;
      break;
    }
    case OverviewTransition::kInOverview:
      animation_type = OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW;
      break;
    case OverviewTransition::kExit:
      NOTREACHED();
  }

  int animate_count = 0;
  bool has_non_cover_animating = false;
  std::vector<OverviewAnimationType> animation_types(rects.size());

  const bool can_do_spawn_animation =
      animate && transition == OverviewTransition::kInOverview;

  for (size_t i = 0; i < window_list_.size(); ++i) {
    OverviewItem* window_item = window_list_[i].get();
    if (ShouldExcludeItemFromGridLayout(window_item, ignored_items)) {
      rects[i].SetRect(0, 0, 0, 0);
      continue;
    }

    // Calculate if each window item needs animation.
    bool should_animate_item = animate;
    // If we're in entering overview process, not all window items in the grid
    // might need animation even if the grid needs animation.
    if (animate && transition == OverviewTransition::kEnter)
      should_animate_item = window_item->should_animate_when_entering();

    if (animate && transition == OverviewTransition::kEnter) {
      if (window_item->should_animate_when_entering() &&
          !has_non_cover_animating) {
        has_non_cover_animating |=
            !CanCoverAvailableWorkspace(window_item->GetWindow());
        ++animate_count;
      }
    }

    if (can_do_spawn_animation && window_item->should_use_spawn_animation())
      animation_type = OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW;

    animation_types[i] =
        should_animate_item ? animation_type : OVERVIEW_ANIMATION_NONE;
  }

  if (animate && transition == OverviewTransition::kEnter &&
      !window_list_.empty()) {
    bool single_animation_in_clamshell =
        animate_count == 1 && !has_non_cover_animating &&
        !Shell::Get()->tablet_mode_controller()->InTabletMode();
    bool minimized_in_tablet = overview_session_->enter_exit_overview_type() ==
                               OverviewEnterExitType::kFadeInEnter;
    metrics_tracker_ = std::make_unique<OverviewEnterMetricsTracker>(
        window_list_[0]->GetWindow()->layer()->GetCompositor(),
        SplitViewController::Get(root_window_)->InSplitViewMode(),
        single_animation_in_clamshell, minimized_in_tablet);
  }

  // Apply the animation after creating metrics_tracker_ so that unit test
  // can correctly count the measure requests.
  for (size_t i = 0; i < window_list_.size(); ++i) {
    if (rects[i].IsEmpty())
      continue;
    OverviewItem* window_item = window_list_[i].get();
    window_item->SetBounds(rects[i], animation_types[i]);
  }
}

OverviewItem* OverviewGrid::GetOverviewItemContaining(
    const aura::Window* window) const {
  for (const auto& window_item : window_list_) {
    if (window_item && window_item->Contains(window))
      return window_item.get();
  }
  return nullptr;
}

void OverviewGrid::AddItem(aura::Window* window,
                           bool reposition,
                           bool animate,
                           const base::flat_set<OverviewItem*>& ignored_items,
                           size_t index,
                           bool use_spawn_animation,
                           bool restack) {
  DCHECK(!GetOverviewItemContaining(window));
  DCHECK_LE(index, window_list_.size());

  window_list_.insert(
      window_list_.begin() + index,
      std::make_unique<OverviewItem>(window, overview_session_, this));

  UpdateFrameThrottling();
  auto* item = window_list_[index].get();
  item->PrepareForOverview();

  if (animate && use_spawn_animation && reposition) {
    item->set_should_use_spawn_animation(true);
  } else {
    // The item is added after overview enter animation is complete, so
    // just call OnStartingAnimationComplete() only if we won't animate it with
    // with the spawn animation. Otherwise, OnStartingAnimationComplete() will
    // be called when the spawn-item-animation completes (See
    // OverviewItem::OnItemSpawnedAnimationCompleted()).
    item->OnStartingAnimationComplete();
  }

  if (restack) {
    if (reposition && animate)
      item->set_should_restack_on_animation_end(true);
    else
      item->Restack();
  }
  if (reposition)
    PositionWindows(animate, ignored_items);
}

void OverviewGrid::AppendItem(aura::Window* window,
                              bool reposition,
                              bool animate,
                              bool use_spawn_animation) {
  AddItem(window, reposition, animate, /*ignored_items=*/{},
          window_list_.size(), use_spawn_animation, /*restack=*/false);
}

void OverviewGrid::AddItemInMruOrder(aura::Window* window,
                                     bool reposition,
                                     bool animate,
                                     bool restack) {
  AddItem(window, reposition, animate, /*ignored_items=*/{},
          FindInsertionIndex(window), /*use_spawn_animation=*/false, restack);
}

void OverviewGrid::RemoveItem(OverviewItem* overview_item,
                              bool item_destroying,
                              bool reposition) {
  EndNudge();

  // Use reverse iterator to be efficient when removing all.
  auto iter = std::find_if(window_list_.rbegin(), window_list_.rend(),
                           base::MatchesUniquePtr(overview_item));
  DCHECK(iter != window_list_.rend());

  // This can also be called when shutting down |this|, at which the item will
  // be cleaning up and its associated view may be nullptr. |overview_item|
  // needs to still be in |window_list_| so we can compute what the deleted
  // index is.
  if (overview_session_ && (*iter)->overview_item_view()) {
    overview_session_->highlight_controller()->OnViewDestroyingOrDisabling(
        (*iter)->overview_item_view());
  }

  // Erase from the list first because deleting OverviewItem can lead to
  // iterating through the |window_list_|.
  std::unique_ptr<OverviewItem> tmp = std::move(*iter);
  window_list_.erase(std::next(iter).base());
  tmp.reset();

  UpdateFrameThrottling();

  if (!item_destroying)
    return;

  if (!overview_session_)
    return;

  if (empty()) {
    overview_session_->OnGridEmpty();
    return;
  }

  if (reposition) {
    // Update the grid bounds if needed and reposition the windows minus the
    // currently overview dragged window, if there is one. Note: this does not
    // update the grid bounds if the window being dragged from the top or shelf,
    // the former being handled in TabletModeWindowDragDelegate's destructor.
    base::flat_set<OverviewItem*> ignored_items;
    OverviewItem* dragged_item =
        overview_session_->GetCurrentDraggedOverviewItem();
    if (dragged_item)
      ignored_items.insert(dragged_item);
    const gfx::Rect grid_bounds = GetGridBoundsInScreen(
        root_window_,
        split_view_drag_indicators_
            ? base::make_optional(
                  split_view_drag_indicators_->current_window_dragging_state())
            : base::nullopt,
        /*divider_changed=*/false,
        /*account_for_hotseat=*/true);
    SetBoundsAndUpdatePositions(grid_bounds, ignored_items, /*animate=*/true);
  }
}

void OverviewGrid::AddDropTargetForDraggingFromThisGrid(
    OverviewItem* dragged_item) {
  DCHECK(!drop_target_widget_);
  drop_target_widget_ =
      CreateDropTargetWidget(root_window_, dragged_item->GetWindow());
  const size_t position = GetOverviewItemIndex(dragged_item) + 1u;
  overview_session_->AddItem(drop_target_widget_->GetNativeWindow(),
                             /*reposition=*/true, /*animate=*/false,
                             /*ignored_items=*/{dragged_item}, position);
}

void OverviewGrid::AddDropTargetNotForDraggingFromThisGrid(
    aura::Window* dragged_window,
    bool animate) {
  DCHECK(!drop_target_widget_);
  drop_target_widget_ = CreateDropTargetWidget(root_window_, dragged_window);
  aura::Window* drop_target_window = drop_target_widget_->GetNativeWindow();
  if (animate) {
    drop_target_widget_->SetOpacity(0.f);
    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_DROP_TARGET_FADE, drop_target_window);
    drop_target_widget_->SetOpacity(1.f);
  }
  const size_t position = FindInsertionIndex(dragged_window);
  overview_session_->AddItem(drop_target_window, /*reposition=*/true, animate,
                             /*ignored_items=*/{}, position);
}

void OverviewGrid::RemoveDropTarget() {
  DCHECK(drop_target_widget_);
  OverviewItem* drop_target = GetDropTarget();
  overview_session_->RemoveItem(drop_target);
  drop_target_widget_.reset();
}

void OverviewGrid::SetBoundsAndUpdatePositions(
    const gfx::Rect& bounds_in_screen,
    const base::flat_set<OverviewItem*>& ignored_items,
    bool animate) {
  bounds_ = bounds_in_screen;
  MaybeUpdateDesksWidgetBounds();
  PositionWindows(animate, ignored_items);
}

void OverviewGrid::RearrangeDuringDrag(
    OverviewItem* dragged_item,
    SplitViewDragIndicators::WindowDraggingState window_dragging_state) {
  OverviewItem* drop_target = GetDropTarget();

  // Update the drop target visibility according to |window_dragging_state|.
  if (drop_target) {
    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_DROP_TARGET_FADE,
        drop_target_widget_->GetNativeWindow());
    drop_target->SetOpacity(GetWantedDropTargetOpacity(window_dragging_state));
  }

  // Update the grid's bounds.
  const gfx::Rect wanted_grid_bounds = GetGridBoundsInScreen(
      root_window_, base::make_optional(window_dragging_state),
      /*divider_changed=*/false, /*account_for_hotseat=*/true);
  if (bounds_ != wanted_grid_bounds) {
    base::flat_set<OverviewItem*> ignored_items;
    if (dragged_item)
      ignored_items.insert(dragged_item);
    SetBoundsAndUpdatePositions(wanted_grid_bounds, ignored_items,
                                /*animate=*/true);
  }
}

void OverviewGrid::SetSplitViewDragIndicatorsDraggedWindow(
    aura::Window* dragged_window) {
  DCHECK(split_view_drag_indicators_);
  split_view_drag_indicators_->SetDraggedWindow(dragged_window);
}

void OverviewGrid::SetSplitViewDragIndicatorsWindowDraggingState(
    SplitViewDragIndicators::WindowDraggingState window_dragging_state) {
  DCHECK(split_view_drag_indicators_);
  split_view_drag_indicators_->SetWindowDraggingState(window_dragging_state);
}

bool OverviewGrid::MaybeUpdateDesksWidgetBounds() {
  if (!desks_widget_)
    return false;

  const gfx::Rect desks_widget_bounds = GetDesksWidgetBounds();
  if (desks_widget_bounds != desks_widget_->GetWindowBoundsInScreen()) {
    // Note that the desks widget window is placed on the active desk container,
    // which has the kUsesScreenCoordinatesKey property set to true, and hence
    // we use the screen coordinates when positioning the desks widget.
    //
    // On certain display zooms, the requested |desks_widget_bounds| may differ
    // than the current screen bounds of the desks widget by 1dp, but internally
    // it will end up being the same and therefore a layout may not be
    // triggered. This can cause mini views not to show up at all. We must
    // guarantee that a layout will always occur by invalidating the layout.
    // See https://crbug.com/1056371 for more details.
    desks_bar_view_->InvalidateLayout();
    desks_widget_->SetBounds(desks_widget_bounds);
    return true;
  }
  return false;
}

void OverviewGrid::UpdateDropTargetBackgroundVisibility(
    OverviewItem* dragged_item,
    const gfx::PointF& location_in_screen) {
  DCHECK(drop_target_widget_);
  aura::Window* target_window =
      GetTargetWindowOnLocation(location_in_screen, dragged_item);
  DropTargetView* drop_target_view =
      static_cast<DropTargetView*>(drop_target_widget_->GetContentsView());
  DCHECK(drop_target_view);
  drop_target_view->UpdateBackgroundVisibility(
      target_window && IsDropTargetWindow(target_window));
}

void OverviewGrid::OnSelectorItemDragStarted(OverviewItem* item) {
  CommitDeskNameChanges();
  for (auto& overview_mode_item : window_list_)
    overview_mode_item->OnSelectorItemDragStarted(item);
}

void OverviewGrid::OnSelectorItemDragEnded(bool snap) {
  for (auto& overview_mode_item : window_list_)
    overview_mode_item->OnSelectorItemDragEnded(snap);
}

void OverviewGrid::OnWindowDragStarted(aura::Window* dragged_window,
                                       bool animate) {
  dragged_window_ = dragged_window;
  AddDropTargetNotForDraggingFromThisGrid(dragged_window, animate);
  // Stack the |dragged_window| at top during drag.
  dragged_window->parent()->StackChildAtTop(dragged_window);
  // Called to set caption and title visibility during dragging.
  OnSelectorItemDragStarted(/*item=*/nullptr);
}

void OverviewGrid::OnWindowDragContinued(
    aura::Window* dragged_window,
    const gfx::PointF& location_in_screen,
    SplitViewDragIndicators::WindowDraggingState window_dragging_state) {
  DCHECK_EQ(dragged_window_, dragged_window);
  DCHECK_EQ(dragged_window->GetRootWindow(), root_window_);

  RearrangeDuringDrag(nullptr, window_dragging_state);
  UpdateDropTargetBackgroundVisibility(nullptr, location_in_screen);

  aura::Window* target_window =
      GetTargetWindowOnLocation(location_in_screen, /*ignored_item=*/nullptr);

  if (SplitViewDragIndicators::GetSnapPosition(window_dragging_state) !=
      SplitViewController::NONE) {
    // If the dragged window is currently dragged into preview window area,
    // hide the highlight.
    overview_session_->highlight_controller()->HideTabDragHighlight();

    // Also clear ash::kIsDeferredTabDraggingTargetWindowKey key on the target
    // overview item so that it can't merge into this overview item if the
    // dragged window is currently in preview window area.
    if (target_window && !IsDropTargetWindow(target_window))
      target_window->ClearProperty(kIsDeferredTabDraggingTargetWindowKey);

    return;
  }

  // Show the tab drag highlight if |location_in_screen| is contained by the
  // browser windows' overview item in overview.
  if (target_window &&
      target_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey)) {
    auto* item = GetOverviewItemContaining(target_window);
    if (!item)
      return;

    overview_session_->highlight_controller()->ShowTabDragHighlight(
        item->overview_item_view());
    return;
  }

  overview_session_->highlight_controller()->HideTabDragHighlight();
}

void OverviewGrid::OnWindowDragEnded(aura::Window* dragged_window,
                                     const gfx::PointF& location_in_screen,
                                     bool should_drop_window_into_overview,
                                     bool snap) {
  DCHECK_EQ(dragged_window_, dragged_window);
  DCHECK_EQ(dragged_window->GetRootWindow(), root_window_);
  DCHECK(drop_target_widget_.get());
  dragged_window_ = nullptr;

  // Add the dragged window into drop target in overview if
  // |should_drop_window_into_overview| is true. Only consider add the dragged
  // window into drop target if SelectedWindow is false since drop target will
  // not be selected and tab dragging might drag a tab window to merge it into a
  // browser window in overview.
  if (overview_session_->highlight_controller()->IsTabDragHighlightVisible())
    overview_session_->highlight_controller()->HideTabDragHighlight();
  else if (should_drop_window_into_overview)
    AddDraggedWindowIntoOverviewOnDragEnd(dragged_window);

  RemoveDropTarget();

  // Called to reset caption and title visibility after dragging.
  OnSelectorItemDragEnded(snap);

  // After drag ends, if the dragged window needs to merge into another window
  // |target_window|, and we may need to update |minimized_widget_| that holds
  // the contents of |target_window| if |target_window| is a minimized window
  // in overview.
  aura::Window* target_window =
      GetTargetWindowOnLocation(location_in_screen, /*ignored_item=*/nullptr);
  if (target_window &&
      target_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey)) {
    // Create an window observer and update the minimized window widget after
    // the dragged window merges into |target_window|.
    if (!target_window_observer_)
      target_window_observer_ = std::make_unique<TargetWindowObserver>();
    target_window_observer_->StartObserving(target_window);
  }

  // Update the grid bounds and reposition windows. Since the grid bounds might
  // be updated based on the preview area during drag, but the window finally
  // didn't be snapped to the preview area.
  SetBoundsAndUpdatePositions(GetGridBoundsInScreen(root_window_),
                              /*ignored_items=*/{},
                              /*animate=*/true);
}

void OverviewGrid::SetVisibleDuringWindowDragging(bool visible, bool animate) {
  for (const auto& window_item : window_list_)
    window_item->SetVisibleDuringWindowDragging(visible, animate);

  // Update |desks_widget_|.
  if (desks_widget_) {
    ui::Layer* layer = desks_widget_->GetNativeWindow()->layer();
    float new_opacity = visible ? 1.f : 0.f;
    if (layer->GetTargetOpacity() == new_opacity)
      return;

    if (animate) {
      ScopedOverviewAnimationSettings settings(
          OVERVIEW_ANIMATION_OPACITY_ON_WINDOW_DRAG,
          desks_widget_->GetNativeWindow());
      layer->SetOpacity(new_opacity);
    } else {
      layer->SetOpacity(new_opacity);
    }
  }
}

bool OverviewGrid::IsDropTargetWindow(aura::Window* window) const {
  return drop_target_widget_ &&
         drop_target_widget_->GetNativeWindow() == window;
}

OverviewItem* OverviewGrid::GetDropTarget() {
  return drop_target_widget_
             ? GetOverviewItemContaining(drop_target_widget_->GetNativeWindow())
             : nullptr;
}

void OverviewGrid::OnDisplayMetricsChanged() {
  if (split_view_drag_indicators_)
    split_view_drag_indicators_->OnDisplayBoundsChanged();

  UpdateCannotSnapWarningVisibility();
  // In case of split view mode, the grid bounds and item positions will be
  // updated in |OnSplitViewDividerPositionChanged|.
  if (SplitViewController::Get(root_window_)->InSplitViewMode())
    return;
  SetBoundsAndUpdatePositions(GetGridBoundsInScreen(root_window_),
                              /*ignored_items=*/{}, /*animate=*/false);
}

void OverviewGrid::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  // Do nothing if overview is being shutdown.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession())
    return;

  SplitViewController* split_view_controller =
      SplitViewController::Get(root_window_);
  const bool unsnappable_window_activated =
      state == SplitViewController::State::kNoSnap &&
      split_view_controller->end_reason() ==
          SplitViewController::EndReason::kUnsnappableWindowActivated;

  // Restore focus unless either a window was just snapped (and activated) or
  // split view mode was ended by activating an unsnappable window.
  if (state != SplitViewController::State::kNoSnap ||
      unsnappable_window_activated) {
    overview_session_->ResetFocusRestoreWindow(false);
  }

  // If two windows were snapped to both sides of the screen or an unsnappable
  // window was just activated, or we're in single split mode in clamshell mode
  // and there is no window in overview, end overview mode and bail out.
  if (state == SplitViewController::State::kBothSnapped ||
      unsnappable_window_activated ||
      (split_view_controller->InClamshellSplitViewMode() &&
       overview_session_->IsEmpty())) {
    overview_controller->EndOverview();
    return;
  }

  // Update the cannot snap warnings and adjust the grid bounds.
  UpdateCannotSnapWarningVisibility();
  SetBoundsAndUpdatePositions(GetGridBoundsInScreen(root_window_),
                              /*ignored_items=*/{}, /*animate=*/false);

  // If split view mode was ended, then activate the overview focus window, to
  // match the behavior of entering overview mode in the beginning.
  if (state == SplitViewController::State::kNoSnap)
    wm::ActivateWindow(overview_session_->GetOverviewFocusWindow());
}

void OverviewGrid::OnSplitViewDividerPositionChanged() {
  SetBoundsAndUpdatePositions(
      GetGridBoundsInScreen(root_window_,
                            /*window_dragging_state=*/base::nullopt,
                            /*divider_changed=*/true,
                            /*account_for_hotseat=*/true),
      /*ignored_items=*/{}, /*animate=*/false);
}

void OverviewGrid::OnScreenCopiedBeforeRotation() {
  Shell::Get()->overview_controller()->PauseOcclusionTracker();

  for (auto& window : window_list()) {
    window->set_disable_mask(true);
    window->UpdateRoundedCornersAndShadow();
    window->StopWidgetAnimation();
  }
}

void OverviewGrid::OnScreenRotationAnimationFinished(
    ScreenRotationAnimator* animator,
    bool canceled) {
  for (auto& window : window_list())
    window->set_disable_mask(false);
  Shell::Get()->overview_controller()->DelayedUpdateRoundedCornersAndShadow();
  Shell::Get()->overview_controller()->UnpauseOcclusionTracker(
      kOcclusionUnpauseDurationForRotation);
}

void OverviewGrid::OnWallpaperChanging() {
  grid_event_handler_.reset();
}

void OverviewGrid::OnWallpaperChanged() {
  grid_event_handler_ = std::make_unique<OverviewGridEventHandler>(this);
}

void OverviewGrid::OnStartingAnimationComplete(bool canceled) {
  metrics_tracker_.reset();
  if (canceled)
    return;

  MaybeInitDesksWidget();

  for (auto& window : window_list())
    window->OnStartingAnimationComplete();

}

void OverviewGrid::CalculateWindowListAnimationStates(
    OverviewItem* selected_item,
    OverviewTransition transition,
    const std::vector<gfx::RectF>& target_bounds) {
  using OverviewTransition = OverviewTransition;

  // Sanity checks to enforce assumptions used in later codes.
  switch (transition) {
    case OverviewTransition::kEnter:
      DCHECK_EQ(target_bounds.size(), window_list_.size());
      break;
    case OverviewTransition::kExit:
      DCHECK(target_bounds.empty());
      break;
    default:
      NOTREACHED();
  }

  // Create a copy of |window_list_| which has always on top windows in the
  // front.
  std::vector<OverviewItem*> items;
  std::transform(
      window_list_.begin(), window_list_.end(), std::back_inserter(items),
      [](const std::unique_ptr<OverviewItem>& item) -> OverviewItem* {
        return item.get();
      });
  // Sort items by:
  // 1) Selected items that are always on top windows.
  // 2) Other always on top windows.
  // 3) Selected items that are not always on top windows.
  // 4) Other not always on top windows.
  // Preserves ordering if the category is the same.
  std::sort(items.begin(), items.end(),
            [&selected_item](OverviewItem* a, OverviewItem* b) {
              // NB: This treats all non-normal z-ordered windows the same. If
              // Aura ever adopts z-order levels, this will need to be changed.
              const bool a_on_top =
                  a->GetWindow()->GetProperty(aura::client::kZOrderingKey) !=
                  ui::ZOrderLevel::kNormal;
              const bool b_on_top =
                  b->GetWindow()->GetProperty(aura::client::kZOrderingKey) !=
                  ui::ZOrderLevel::kNormal;
              if (selected_item && a_on_top && b_on_top)
                return a == selected_item;
              if (a_on_top)
                return true;
              if (b_on_top)
                return false;
              if (selected_item)
                return a == selected_item;
              return false;
            });

  SkRegion occluded_region;
  auto* split_view_controller = SplitViewController::Get(root_window_);
  if (split_view_controller->InSplitViewMode()) {
    // Snapped windows and the split view divider are not included in
    // |target_bounds| or |window_list_|, but can occlude other windows, so add
    // them manually to |region| here.
    SkIRect snapped_window_bounds = gfx::RectToSkIRect(
        split_view_controller->GetDefaultSnappedWindow()->GetBoundsInScreen());
    occluded_region.op(snapped_window_bounds, SkRegion::kUnion_Op);

    auto* divider = split_view_controller->split_view_divider();
    if (divider) {
      aura::Window* divider_window =
          divider->divider_widget()->GetNativeWindow();
      SkIRect divider_bounds =
          gfx::RectToSkIRect(divider_window->GetBoundsInScreen());
      occluded_region.op(divider_bounds, SkRegion::kUnion_Op);
    }
  }

  gfx::Rect screen_bounds = GetGridEffectiveBounds();
  for (size_t i = 0; i < items.size(); ++i) {
    const bool minimized =
        WindowState::Get(items[i]->GetWindow())->IsMinimized();
    bool src_occluded = minimized;
    bool dst_occluded = false;
    gfx::Rect src_bounds_temp =
        minimized ? gfx::Rect()
                  : items[i]->GetWindow()->GetBoundsInRootWindow();
    if (!src_bounds_temp.IsEmpty()) {
      if (transition == OverviewTransition::kEnter &&
          Shell::Get()->tablet_mode_controller()->InTabletMode()) {
        BackdropController* backdrop_controller =
            GetActiveWorkspaceController(root_window_)
                ->layout_manager()
                ->backdrop_controller();
        if (backdrop_controller->GetTopmostWindowWithBackdrop() ==
            items[i]->GetWindow()) {
          src_bounds_temp = screen_util::GetDisplayWorkAreaBoundsInParent(
              items[i]->GetWindow());
        }
      } else if (transition == OverviewTransition::kExit) {
        // On exiting overview, |GetBoundsInRootWindow()| will have the overview
        // translation applied to it, so use |bounds()| and
        // |ConvertRectToScreen()| to get the true target bounds.
        src_bounds_temp = items[i]->GetWindow()->bounds();
        ::wm::ConvertRectToScreen(items[i]->root_window(), &src_bounds_temp);
      }
    }

    // The bounds of of the destination may be partially or fully offscreen.
    // Partially offscreen rects should be clipped so the onscreen portion is
    // treated normally. Fully offscreen rects (intersection with the screen
    // bounds is empty) should never be animated.
    gfx::Rect dst_bounds_temp = gfx::ToEnclosedRect(
        transition == OverviewTransition::kEnter ? target_bounds[i]
                                                 : items[i]->target_bounds());
    dst_bounds_temp.Intersect(screen_bounds);
    if (dst_bounds_temp.IsEmpty()) {
      items[i]->set_should_animate_when_entering(false);
      items[i]->set_should_animate_when_exiting(false);
      continue;
    }

    SkIRect src_bounds = gfx::RectToSkIRect(src_bounds_temp);
    SkIRect dst_bounds = gfx::RectToSkIRect(dst_bounds_temp);
    if (!occluded_region.isEmpty()) {
      src_occluded |=
          (!src_bounds.isEmpty() && occluded_region.contains(src_bounds));
      dst_occluded |= occluded_region.contains(dst_bounds);
    }

    // Add |src_bounds| to our region if it is not empty (minimized window).
    if (!src_bounds.isEmpty())
      occluded_region.op(src_bounds, SkRegion::kUnion_Op);

    const bool should_animate = !(src_occluded && dst_occluded);
    if (transition == OverviewTransition::kEnter)
      items[i]->set_should_animate_when_entering(should_animate);
    else if (transition == OverviewTransition::kExit)
      items[i]->set_should_animate_when_exiting(should_animate);
  }
}

void OverviewGrid::SetWindowListNotAnimatedWhenExiting() {
  should_animate_when_exiting_ = false;
  for (const auto& item : window_list_)
    item->set_should_animate_when_exiting(false);
}

void OverviewGrid::StartNudge(OverviewItem* item) {
  // When there is one window left, there is no need to nudge.
  if (window_list_.size() <= 1) {
    nudge_data_.clear();
    return;
  }

  // If any of the items are being animated to close, do not nudge any windows
  // otherwise we have to deal with potential items getting removed from
  // |window_list_| midway through a nudge.
  for (const auto& window_item : window_list_) {
    if (window_item->animating_to_close()) {
      nudge_data_.clear();
      return;
    }
  }

  DCHECK(item);

  // Get the bounds of the windows currently, and the bounds if |item| were to
  // be removed.
  std::vector<gfx::RectF> src_rects;
  for (const auto& window_item : window_list_)
    src_rects.push_back(window_item->target_bounds());

  std::vector<gfx::RectF> dst_rects = GetWindowRects({item});

  const size_t index = GetOverviewItemIndex(item);

  // Returns a vector of integers indicating which row the item is in. |index|
  // is the index of the element which is going to be deleted and should not
  // factor into calculations. The call site should mark |index| as -1 if it
  // should not be used. The item at |index| is marked with a 0. The heights of
  // items are all set to the same value so a new row is determined if the y
  // value has changed from the previous item.
  auto get_rows = [](const std::vector<gfx::RectF>& bounds_list, size_t index) {
    std::vector<int> row_numbers;
    int current_row = 1;
    float last_y = 0;
    for (size_t i = 0; i < bounds_list.size(); ++i) {
      if (i == index) {
        row_numbers.push_back(0);
        continue;
      }

      // Update |current_row| if the y position has changed (heights are all
      // equal in overview, so a new y position indicates a new row).
      if (last_y != 0 && last_y != bounds_list[i].y())
        ++current_row;

      row_numbers.push_back(current_row);
      last_y = bounds_list[i].y();
    }

    return row_numbers;
  };

  std::vector<int> src_rows = get_rows(src_rects, -1);
  std::vector<int> dst_rows = get_rows(dst_rects, index);

  // Do nothing if the number of rows change.
  if (dst_rows.back() != 0 && src_rows.back() != dst_rows.back())
    return;
  size_t second_last_index = src_rows.size() - 2;
  if (dst_rows.back() == 0 &&
      src_rows[second_last_index] != dst_rows[second_last_index]) {
    return;
  }

  // Do nothing if the last item from the previous row will drop onto the
  // current row, this will cause the items in the current row to shift to the
  // right while the previous item stays in the previous row, which looks weird.
  if (src_rows[index] > 1) {
    // Find the last item from the previous row.
    size_t previous_row_last_index = index;
    while (src_rows[previous_row_last_index] == src_rows[index]) {
      --previous_row_last_index;
    }

    // Early return if the last item in the previous row changes rows.
    if (src_rows[previous_row_last_index] != dst_rows[previous_row_last_index])
      return;
  }

  // Helper to check whether the item at |item_index| will be nudged.
  auto should_nudge = [&src_rows, &dst_rows, &index](size_t item_index) {
    // Out of bounds.
    if (item_index >= src_rows.size())
      return false;

    // Nudging happens when the item stays on the same row and is also on the
    // same row as the item to be deleted was.
    if (dst_rows[item_index] == src_rows[index] &&
        dst_rows[item_index] == src_rows[item_index]) {
      return true;
    }

    return false;
  };

  // Starting from |index| go up and down while the nudge condition returns
  // true.
  std::vector<int> affected_indexes;
  size_t loop_index;

  if (index > 0) {
    loop_index = index - 1;
    while (should_nudge(loop_index)) {
      affected_indexes.push_back(loop_index);
      --loop_index;
    }
  }

  loop_index = index + 1;
  while (should_nudge(loop_index)) {
    affected_indexes.push_back(loop_index);
    ++loop_index;
  }

  // Populate |nudge_data_| with the indexes in |affected_indexes| and their
  // respective source and destination bounds.
  nudge_data_.resize(affected_indexes.size());
  for (size_t i = 0; i < affected_indexes.size(); ++i) {
    NudgeData data;
    data.index = affected_indexes[i];
    data.src = src_rects[data.index];
    data.dst = dst_rects[data.index];
    nudge_data_[i] = data;
  }
}

void OverviewGrid::UpdateNudge(OverviewItem* item, double value) {
  for (const auto& data : nudge_data_) {
    DCHECK_LT(data.index, window_list_.size());

    OverviewItem* nudged_item = window_list_[data.index].get();
    double nudge_param = value * value / 30.0;
    nudge_param = base::ClampToRange(nudge_param, 0.0, 1.0);
    gfx::RectF bounds =
        gfx::Tween::RectFValueBetween(nudge_param, data.src, data.dst);
    nudged_item->SetBounds(bounds, OVERVIEW_ANIMATION_NONE);
  }
}

void OverviewGrid::EndNudge() {
  nudge_data_.clear();
}

void OverviewGrid::SlideWindowsIn() {
  for (const auto& window_item : window_list_)
    window_item->SlideWindowIn();
}

std::unique_ptr<ui::ScopedLayerAnimationSettings>
OverviewGrid::UpdateYPositionAndOpacity(
    float new_y,
    float opacity,
    OverviewSession::UpdateAnimationSettingsCallback callback) {
  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings_to_observe;
  if (desks_widget_) {
    aura::Window* window = desks_widget_->GetNativeWindow();
    ui::Layer* layer = window->layer();
    if (!callback.is_null()) {
      settings_to_observe = std::make_unique<ui::ScopedLayerAnimationSettings>(
          layer->GetAnimator());
      callback.Run(settings_to_observe.get());
    }
    window->SetTransform(gfx::Transform(1.f, 0.f, 0.f, 1.f, 0.f, -new_y));
    layer->SetOpacity(opacity);
  }

  // Translate the window items to |new_y| with the opacity. Observe the
  // animation of the last item, if any.
  for (const auto& window_item : window_list_) {
    auto new_settings =
        window_item->UpdateYPositionAndOpacity(new_y, opacity, callback);
    if (new_settings)
      settings_to_observe = std::move(new_settings);
  }
  return settings_to_observe;
}

aura::Window* OverviewGrid::GetTargetWindowOnLocation(
    const gfx::PointF& location_in_screen,
    OverviewItem* ignored_item) {
  for (std::unique_ptr<OverviewItem>& item : window_list_) {
    if (item.get() == ignored_item)
      continue;
    if (item->target_bounds().Contains(location_in_screen))
      return item->GetWindow();
  }
  return nullptr;
}

bool OverviewGrid::IsDesksBarViewActive() const {
  DCHECK(desks_util::ShouldDesksBarBeCreated());

  // The desk bar view is not active if there is only a single desk when
  // overview is started. Once there are more than one desk, it should stay
  // active even if the 2nd to last desk is deleted.
  return DesksController::Get()->desks().size() > 1 ||
         (desks_bar_view_ && !desks_bar_view_->mini_views().empty());
}

gfx::Rect OverviewGrid::GetGridEffectiveBounds() const {
  if (!desks_util::ShouldDesksBarBeCreated() || !IsDesksBarViewActive())
    return bounds_;

  gfx::Rect effective_bounds = bounds_;
  effective_bounds.Inset(0,
                         DesksBarView::GetBarHeightForWidth(
                             root_window_, desks_bar_view_, bounds_.width()),
                         0, 0);
  return effective_bounds;
}

bool OverviewGrid::IntersectsWithDesksBar(const gfx::Point& screen_location,
                                          bool update_desks_bar_drag_details,
                                          bool for_drop) {
  DCHECK(desks_util::ShouldDesksBarBeCreated());

  const bool dragged_item_over_bar =
      desks_widget_->GetWindowBoundsInScreen().Contains(screen_location);
  if (update_desks_bar_drag_details) {
    desks_bar_view_->SetDragDetails(screen_location,
                                    !for_drop && dragged_item_over_bar);
  }
  return dragged_item_over_bar;
}

bool OverviewGrid::MaybeDropItemOnDeskMiniView(
    const gfx::Point& screen_location,
    OverviewItem* drag_item) {
  DCHECK(desks_util::ShouldDesksBarBeCreated());

  // End the drag for the DesksBarView.
  if (!IntersectsWithDesksBar(screen_location,
                              /*update_desks_bar_drag_details=*/true,
                              /*for_drop=*/true)) {
    return false;
  }

  auto* desks_controller = DesksController::Get();
  for (auto* mini_view : desks_bar_view_->mini_views()) {
    if (!mini_view->IsPointOnMiniView(screen_location))
      continue;

    aura::Window* const dragged_window = drag_item->GetWindow();
    Desk* const target_desk = mini_view->desk();
    if (target_desk == desks_controller->active_desk())
      return false;

    return desks_controller->MoveWindowFromActiveDeskTo(
        dragged_window, target_desk, root_window_,
        DesksMoveWindowFromActiveDeskSource::kDragAndDrop);
  }

  return false;
}

void OverviewGrid::StartScroll() {
  Shell::Get()->overview_controller()->PauseOcclusionTracker();

  // Users are not allowed to scroll past the leftmost or rightmost bounds of
  // the items on screen in the grid. |scroll_offset_min_| is the amount needed
  // to fit the rightmost window into |total_bounds|. The max is zero which is
  // default because windows are aligned to the left from the beginning.
  gfx::Rect total_bounds = GetGridEffectiveBounds();
  total_bounds.Inset(GetGridInsets(total_bounds));

  float rightmost_window_right = 0;
  items_scrolling_bounds_.resize(window_list_.size());
  for (size_t i = 0; i < items_scrolling_bounds_.size(); ++i) {
    const gfx::RectF bounds = window_list_[i]->target_bounds();
    if (rightmost_window_right < bounds.right())
      rightmost_window_right = bounds.right();

    items_scrolling_bounds_[i] = bounds;
  }

  // |rightmost_window_right| may have been modified by an earlier scroll.
  // |scroll_offset_| is added to adjust for that.
  rightmost_window_right -= scroll_offset_;
  scroll_offset_min_ = total_bounds.right() - rightmost_window_right;

  presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
      const_cast<ui::Compositor*>(root_window()->layer()->GetCompositor()),
      kOverviewScrollHistogram, kOverviewScrollMaxLatencyHistogram);
}

bool OverviewGrid::UpdateScrollOffset(float delta) {
  float new_scroll_offset = scroll_offset_;
  new_scroll_offset += delta;
  new_scroll_offset =
      base::ClampToRange(new_scroll_offset, scroll_offset_min_, 0.f);

  // For flings, we want to return false if we hit one of the edges, which is
  // when |new_scroll_offset| is exactly 0.f or |scroll_offset_min_|.
  const bool in_range =
      new_scroll_offset < 0.f && new_scroll_offset > scroll_offset_min_;
  if (new_scroll_offset == scroll_offset_)
    return in_range;

  // Update the bounds of the items which are currently visible on screen.
  DCHECK_EQ(items_scrolling_bounds_.size(), window_list_.size());
  for (size_t i = 0; i < items_scrolling_bounds_.size(); ++i) {
    const gfx::RectF previous_bounds = items_scrolling_bounds_[i];
    items_scrolling_bounds_[i].Offset(new_scroll_offset - scroll_offset_, 0.f);
    const gfx::RectF new_bounds = items_scrolling_bounds_[i];
    if (gfx::RectF(GetGridEffectiveBounds()).Intersects(new_bounds) ||
        gfx::RectF(GetGridEffectiveBounds()).Intersects(previous_bounds)) {
      window_list_[i]->SetBounds(new_bounds, OVERVIEW_ANIMATION_NONE);
    }
  }

  scroll_offset_ = new_scroll_offset;

  DCHECK(presentation_time_recorder_);
  presentation_time_recorder_->RequestNext();
  return in_range;
}

void OverviewGrid::EndScroll() {
  Shell::Get()->overview_controller()->UnpauseOcclusionTracker(
      kOcclusionUnpauseDurationForScroll);
  items_scrolling_bounds_.clear();
  presentation_time_recorder_.reset();

  if (!overview_session_->is_shutting_down())
    PositionWindows(/*animate=*/false);
}

int OverviewGrid::CalculateWidthAndMaybeSetUnclippedBounds(OverviewItem* item,
                                                           int height) {
  const gfx::Size item_size(0, height);
  gfx::SizeF target_size = item->GetTargetBoundsInScreen().size();
  float scale = item->GetItemScale(item_size);
  OverviewGridWindowFillMode grid_fill_mode = item->GetWindowDimensionsType();

  // The drop target, unlike the other windows has its bounds set directly, so
  // |GetTargetBoundsInScreen()| won't return the value we want. Instead, get
  // the scale from the window it was meant to be a placeholder for.
  if (IsDropTargetWindow(item->GetWindow())) {
    aura::Window* dragged_window = nullptr;
    OverviewItem* grid_dragged_item =
        overview_session_->window_drag_controller()
            ? overview_session_->window_drag_controller()->item()
            : nullptr;
    if (grid_dragged_item)
      dragged_window = grid_dragged_item->GetWindow();
    else if (dragged_window_)
      dragged_window = dragged_window_;
    if (dragged_window && dragged_window->parent()) {
      const gfx::Size work_area_size =
          screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
              root_window_)
              .size();
      if (WindowState::Get(dragged_window)->IsMaximized()) {
        grid_fill_mode = ScopedOverviewTransformWindow::GetWindowDimensionsType(
            work_area_size);
        target_size = gfx::SizeF(work_area_size);
      } else {
        gfx::Size dragged_window_size = dragged_window->bounds().size();
        // If the drag started from a different root window, |dragged_window|
        // may not fit into the work area of |root_window_|. Then if
        // |dragged_window| is dropped into this grid, |dragged_window| will
        // shrink to fit into this work area. The drop target shall reflect
        // that.
        dragged_window_size.SetToMin(work_area_size);
        grid_fill_mode = ScopedOverviewTransformWindow::GetWindowDimensionsType(
            dragged_window_size);
        target_size = ::ash::GetTargetBoundsInScreen(dragged_window).size();
        target_size.SetToMin(gfx::SizeF(work_area_size));
      }
      const gfx::SizeF inset_size(0, height);
      scale = ScopedOverviewTransformWindow::GetItemScale(
          target_size, inset_size,
          dragged_window->GetProperty(aura::client::kTopViewInset),
          kHeaderHeightDp);
    }
  }

  int width = std::max(1, base::ClampFloor(target_size.width() * scale));
  switch (grid_fill_mode) {
    case OverviewGridWindowFillMode::kLetterBoxed:
      width = kExtremeWindowRatioThreshold * height;
      break;
    case OverviewGridWindowFillMode::kPillarBoxed:
      width = height / kExtremeWindowRatioThreshold;
      break;
    default:
      break;
  }

  // Get the bounds of the window if there is a snapped window or a window
  // about to be snapped.
  base::Optional<gfx::RectF> split_view_bounds =
      GetSplitviewBoundsMaintainingAspectRatio();
  if (!split_view_bounds) {
    item->set_unclipped_size(base::nullopt);
    return width;
  }

  // Perform horizontal clipping if the window's aspect ratio is wider than the
  // split view bounds aspect ratio, and vertical clipping otherwise.
  const float aspect_ratio =
      target_size.width() /
      (target_size.height() -
       item->GetWindow()->GetProperty(aura::client::kTopViewInset));
  const float target_aspect_ratio =
      split_view_bounds->width() / split_view_bounds->height();
  const bool clip_horizontally = aspect_ratio > target_aspect_ratio;
  const int window_height = height - kHeaderHeightDp;
  gfx::Size unclipped_size;
  if (clip_horizontally) {
    unclipped_size.set_width(width);
    unclipped_size.set_height(height);
    // For horizontal clipping, shrink |width| so that the aspect ratio matches
    // that of |split_view_bounds|.
    width = std::max(1, base::ClampFloor(target_aspect_ratio * window_height));
  } else {
    // For vertical clipping, we want |height| to stay the same, so calculate
    // what the unclipped height would be based on |split_view_bounds|.

    // Find the width so that it matches height and matches the aspect ratio of
    // |split_view_bounds|.
    width = split_view_bounds->width() * window_height /
            split_view_bounds->height();
    // The unclipped height is the height which matches |width| but keeps the
    // aspect ratio of |target_bounds|. Clipping takes the overview header into
    // account, so add that back in.
    const int unclipped_height =
        width * target_size.height() / target_size.width();
    unclipped_size.set_width(width);
    unclipped_size.set_height(unclipped_height + kHeaderHeightDp);
  }

  DCHECK(!unclipped_size.IsEmpty());
  item->set_unclipped_size(base::make_optional(unclipped_size));
  return width;
}

void OverviewGrid::OnDesksChanged() {
  if (MaybeUpdateDesksWidgetBounds())
    PositionWindows(/*animate=*/false, /*ignored_items=*/{});
  else
    desks_bar_view_->Layout();
}

bool OverviewGrid::IsDeskNameBeingModified() const {
  return desks_bar_view_ && desks_bar_view_->IsDeskNameBeingModified();
}

void OverviewGrid::CommitDeskNameChanges() {
  // The desks bar widget may not be ready, since it is created asynchronously
  // later when the entering overview animations finish.
  if (desks_widget_)
    DeskNameView::CommitChanges(desks_widget_.get());
}

void OverviewGrid::MaybeInitDesksWidget() {
  if (!desks_util::ShouldDesksBarBeCreated() || desks_widget_)
    return;

  desks_widget_ =
      DesksBarView::CreateDesksWidget(root_window_, GetDesksWidgetBounds());

  // The following order of function calls is significant: SetContentsView()
  // must be called before DesksBarView:: Init(). This is needed because the
  // desks mini views need to access the widget to get the root window in order
  // to know how to layout themselves.
  desks_bar_view_ =
      desks_widget_->SetContentsView(std::make_unique<DesksBarView>(this));
  desks_bar_view_->Init();

  desks_widget_->Show();

  // TODO(afakhry): Check if we need to keep this as the bottom-most window in
  // the container.
  auto* window = desks_widget_->GetNativeWindow();
  window->parent()->StackChildAtBottom(window);
}

std::vector<gfx::RectF> OverviewGrid::GetWindowRects(
    const base::flat_set<OverviewItem*>& ignored_items) {
  gfx::Rect total_bounds = GetGridEffectiveBounds();

  // Windows occupy vertically centered area with additional vertical insets.
  total_bounds.Inset(GetGridInsets(total_bounds));
  std::vector<gfx::RectF> rects;

  // Keep track of the lowest coordinate.
  int max_bottom = total_bounds.y();

  // Right bound of the narrowest row.
  int min_right = total_bounds.right();
  // Right bound of the widest row.
  int max_right = total_bounds.x();

  // Keep track of the difference between the narrowest and the widest row.
  // Initially this is set to the worst it can ever be assuming the windows fit.
  int width_diff = total_bounds.width();

  // Initially allow the windows to occupy all available width. Shrink this
  // available space horizontally to find the breakdown into rows that achieves
  // the minimal |width_diff|.
  int right_bound = total_bounds.right();

  // Determine the optimal height bisecting between |low_height| and
  // |high_height|. Once this optimal height is known, |height_fixed| is set to
  // true and the rows are balanced by repeatedly squeezing the widest row to
  // cause windows to overflow to the subsequent rows.
  int low_height = 2 * kWindowMargin;
  int high_height = std::max(low_height, total_bounds.height() + 1);
  int height = 0.5 * (low_height + high_height);
  bool height_fixed = false;

  // Repeatedly try to fit the windows |rects| within |right_bound|.
  // If a maximum |height| is found such that all window |rects| fit, this
  // fitting continues while shrinking the |right_bound| in order to balance the
  // rows. If the windows fit the |right_bound| would have been decremented at
  // least once so it needs to be incremented once before getting out of this
  // loop and one additional pass made to actually fit the |rects|.
  // If the |rects| cannot fit (e.g. there are too many windows) the bisection
  // will still finish and we might increment the |right_bound| once pixel extra
  // which is acceptable since there is an unused margin on the right.
  bool make_last_adjustment = false;
  while (true) {
    gfx::Rect overview_mode_bounds(total_bounds);
    overview_mode_bounds.set_width(right_bound - total_bounds.x());
    bool windows_fit = FitWindowRectsInBounds(
        overview_mode_bounds, std::min(kMaxHeight, height), ignored_items,
        &rects, &max_bottom, &min_right, &max_right);

    if (height_fixed) {
      if (!windows_fit) {
        // Revert the previous change to |right_bound| and do one last pass.
        right_bound++;
        make_last_adjustment = true;
        break;
      }
      // Break if all the windows are zero-width at the current scale.
      if (max_right <= total_bounds.x())
        break;
    } else {
      // Find the optimal row height bisecting between |low_height| and
      // |high_height|.
      if (windows_fit)
        low_height = height;
      else
        high_height = height;
      height = 0.5 * (low_height + high_height);
      // When height can no longer be improved, start balancing the rows.
      if (height == low_height)
        height_fixed = true;
    }

    if (windows_fit && height_fixed) {
      if (max_right - min_right <= width_diff) {
        // Row alignment is getting better. Try to shrink the |right_bound| in
        // order to squeeze the widest row.
        right_bound = max_right - 1;
        width_diff = max_right - min_right;
      } else {
        // Row alignment is getting worse.
        // Revert the previous change to |right_bound| and do one last pass.
        right_bound++;
        make_last_adjustment = true;
        break;
      }
    }
  }
  // Once the windows in |window_list_| no longer fit, the change to
  // |right_bound| was reverted. Perform one last pass to position the |rects|.
  if (make_last_adjustment) {
    gfx::Rect overview_mode_bounds(total_bounds);
    overview_mode_bounds.set_width(right_bound - total_bounds.x());
    FitWindowRectsInBounds(overview_mode_bounds, std::min(kMaxHeight, height),
                           ignored_items, &rects, &max_bottom, &min_right,
                           &max_right);
  }

  gfx::Vector2dF offset(0, (total_bounds.bottom() - max_bottom) / 2.f);
  for (size_t i = 0; i < rects.size(); ++i)
    rects[i] += offset;
  return rects;
}

std::vector<gfx::RectF> OverviewGrid::GetWindowRectsForTabletModeLayout(
    const base::flat_set<OverviewItem*>& ignored_items) {
  gfx::Rect total_bounds = GetGridEffectiveBounds();
  // Windows occupy vertically centered area with additional vertical insets.
  total_bounds.Inset(GetGridInsets(total_bounds));

  // |scroll_offset_min_| may be changed on positioning (either by closing
  // windows or display changes). Recalculate it and clamp |scroll_offset_|, so
  // that the items are always aligned left or right.
  float rightmost_window_right = 0;
  for (const auto& item : window_list_) {
    if (ShouldExcludeItemFromGridLayout(item.get(), ignored_items))
      continue;
    rightmost_window_right =
        std::max(rightmost_window_right, item->target_bounds().right());
  }

  // |rightmost_window_right| may have been modified by an earlier scroll.
  // |scroll_offset_| is added to adjust for that.
  rightmost_window_right -= scroll_offset_;
  scroll_offset_min_ = total_bounds.right() - rightmost_window_right;
  scroll_offset_ = base::ClampToRange(scroll_offset_, scroll_offset_min_, 0.f);

  // Map which contains up to |kTabletLayoutRow| entries with information on the
  // last items right bound per row. Used so we can place the next item directly
  // next to the last item. The key is the y-value of the row, and the value is
  // the rightmost x-value.
  base::flat_map<float, float> right_edge_map;

  // Since the number of rows is limited, windows are laid out column-wise so
  // that the most recently used windows are displayed first. When the dragged
  // item becomes an |ignored_item|, move the other windows accordingly.
  // |window_position| matches the positions of the windows' indexes from
  // |window_list_|. However, if a window turns out to be an ignored item,
  // |window_position| remains where the item was as to then reposition the
  // other window's bounds in place of that item.
  const int height = total_bounds.height() / kTabletLayoutRow;
  int window_position = 0;
  std::vector<gfx::RectF> rects;
  for (size_t i = 0; i < window_list_.size(); ++i) {
    OverviewItem* item = window_list_[i].get();
    if (ShouldExcludeItemFromGridLayout(item, ignored_items)) {
      rects.push_back(gfx::RectF());
      continue;
    }

    // Calculate the width and y position of the item.
    const int width =
        CalculateWidthAndMaybeSetUnclippedBounds(window_list_[i].get(), height);
    const int y =
        height * (window_position % kTabletLayoutRow) + total_bounds.y();

    // Use the right bounds of the item next to in the row as the x position, if
    // that item exists.
    const int x = right_edge_map.contains(y)
                      ? right_edge_map[y]
                      : total_bounds.x() + scroll_offset_;
    right_edge_map[y] = x + width;
    DCHECK_LE(int{right_edge_map.size()}, kTabletLayoutRow);

    const gfx::RectF bounds(x, y, width, height);
    rects.push_back(bounds);
    ++window_position;
  }

  return rects;
}

bool OverviewGrid::FitWindowRectsInBounds(
    const gfx::Rect& bounds,
    int height,
    const base::flat_set<OverviewItem*>& ignored_items,
    std::vector<gfx::RectF>* out_rects,
    int* out_max_bottom,
    int* out_min_right,
    int* out_max_right) {
  const size_t window_count = window_list_.size();
  out_rects->resize(window_count);

  // Start in the top-left corner of |bounds|.
  int left = bounds.x();
  int top = bounds.y();

  // Keep track of the lowest coordinate.
  *out_max_bottom = bounds.y();

  // Right bound of the narrowest row.
  *out_min_right = bounds.right();
  // Right bound of the widest row.
  *out_max_right = bounds.x();

  // All elements are of same height and only the height is necessary to
  // determine each item's scale.
  for (size_t i = 0u; i < window_count; ++i) {
    if (ShouldExcludeItemFromGridLayout(window_list_[i].get(), ignored_items))
      continue;

    int width = CalculateWidthAndMaybeSetUnclippedBounds(window_list_[i].get(),
                                                         height) +
                2 * kWindowMargin;
    int height_with_margin = height + 2 * kWindowMargin;

    if (left + width > bounds.right()) {
      // Move to the next row if possible.
      if (*out_min_right > left)
        *out_min_right = left;
      if (*out_max_right < left)
        *out_max_right = left;
      top += height_with_margin;

      // Check if the new row reaches the bottom or if the first item in the new
      // row does not fit within the available width.
      if (top + height_with_margin > bounds.bottom() ||
          bounds.x() + width > bounds.right()) {
        return false;
      }
      left = bounds.x();
    }

    // Position the current rect.
    (*out_rects)[i] = gfx::RectF(left, top, width, height_with_margin);

    // Increment horizontal position using sanitized positive |width|.
    left += width;

    *out_max_bottom = top + height_with_margin;
  }

  // Update the narrowest and widest row width for the last row.
  if (*out_min_right > left)
    *out_min_right = left;
  if (*out_max_right < left)
    *out_max_right = left;

  return true;
}

size_t OverviewGrid::GetOverviewItemIndex(OverviewItem* item) const {
  auto iter = std::find_if(window_list_.begin(), window_list_.end(),
                           base::MatchesUniquePtr(item));
  DCHECK(iter != window_list_.end());
  return iter - window_list_.begin();
}

size_t OverviewGrid::FindInsertionIndex(const aura::Window* window) {
  size_t index = 0u;
  for (aura::Window* mru_window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    if (index == size() ||
        IsDropTargetWindow(window_list_[index]->GetWindow()) ||
        mru_window == window) {
      return index;
    }
    // As we iterate over the whole MRU window list, the windows in this grid
    // will be encountered in the same order, but possibly with other windows in
    // between. Ignore those other windows, and only increment |index| when we
    // reach the next window in this grid.
    if (mru_window == window_list_[index]->GetWindow())
      ++index;
  }
  NOTREACHED();
  return 0u;
}

void OverviewGrid::AddDraggedWindowIntoOverviewOnDragEnd(
    aura::Window* dragged_window) {
  DCHECK(overview_session_);
  if (overview_session_->IsWindowInOverview(dragged_window))
    return;

  // Update the dragged window's bounds before adding it to overview. The
  // dragged window might have resized to a smaller size if the drag
  // happens on tab(s).
  if (window_util::IsDraggingTabs(dragged_window)) {
    const gfx::Rect old_bounds = dragged_window->bounds();
    // We need to temporarily disable the dragged window's ability to merge
    // into another window when changing the dragged window's bounds, so
    // that the dragged window doesn't merge into another window because of
    // its changed bounds.
    dragged_window->SetProperty(kCanAttachToAnotherWindowKey, false);
    TabletModeWindowState::UpdateWindowPosition(
        WindowState::Get(dragged_window), /*animate=*/false);
    const gfx::Rect new_bounds = dragged_window->bounds();
    if (old_bounds != new_bounds) {
      // It's for smoother animation.
      const gfx::Transform transform = gfx::TransformBetweenRects(
          gfx::RectF(new_bounds), gfx::RectF(old_bounds));
      dragged_window->SetTransform(transform);
    }
    dragged_window->ClearProperty(kCanAttachToAnotherWindowKey);
  }

  overview_session_->AddItemInMruOrder(dragged_window, /*reposition=*/false,
                                       /*animate=*/false, /*restack=*/true);
}

gfx::Rect OverviewGrid::GetDesksWidgetBounds() const {
  gfx::Rect desks_widget_screen_bounds = bounds_;
  desks_widget_screen_bounds.set_height(DesksBarView::GetBarHeightForWidth(
      root_window_, desks_bar_view_, desks_widget_screen_bounds.width()));
  // Shift the widget down to make room for the splitview indicator guidance
  // when it's shown at the top of the screen and no other windows are snapped.
  if (split_view_drag_indicators_ &&
      split_view_drag_indicators_->current_window_dragging_state() ==
          SplitViewDragIndicators::WindowDraggingState::kFromOverview &&
      !SplitViewController::IsLayoutHorizontal() &&
      !SplitViewController::Get(root_window_)->InSplitViewMode()) {
    desks_widget_screen_bounds.Offset(
        0, split_view_drag_indicators_->GetLeftHighlightViewBounds().height() +
               2 * kHighlightScreenEdgePaddingDp);
  }

  return screen_util::SnapBoundsToDisplayEdge(desks_widget_screen_bounds,
                                              root_window_);
}

void OverviewGrid::UpdateCannotSnapWarningVisibility() {
  for (auto& overview_mode_item : window_list_)
    overview_mode_item->UpdateCannotSnapWarningVisibility();
}

void OverviewGrid::UpdateFrameThrottling() {
  std::vector<aura::Window*> windows_to_throttle(window_list_.size(), nullptr);
  std::transform(
      window_list_.begin(), window_list_.end(), windows_to_throttle.begin(),
      [](std::unique_ptr<OverviewItem>& item) { return item->GetWindow(); });
  Shell::Get()->frame_throttling_controller()->StartThrottling(
      windows_to_throttle);
}
}  // namespace ash

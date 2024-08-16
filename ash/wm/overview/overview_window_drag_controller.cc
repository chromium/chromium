// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/wm/overview/overview_window_drag_controller.h"

#include <algorithm>

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk_icon_button.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/scoped_float_container_stacker.h"
#include "ash/wm/snap_group/snap_group_metrics.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_constants.h"
#include "base/auto_reset.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The amount of distance from the start of drag the item needs to be dragged
// vertically for it to be closed on release.
constexpr float kDragToCloseDistanceThresholdDp = 160.f;

// The minimum distance that will be considered as a drag event.
constexpr float kMinimumDragDistanceDp = 5.f;
// Items dragged to within |kDistanceFromEdgeDp| of the screen will get snapped
// even if they have not moved by |kMinimumDragToSnapDistanceDp|.
constexpr float kDistanceFromEdgeDp = 16.f;
// The minimum distance that an item must be moved before it is snapped. This
// prevents accidental snaps.
constexpr float kMinimumDragToSnapDistanceDp = 96.f;

// Flings with less velocity than this will not close the dragged item.
constexpr float kFlingToCloseVelocityThreshold = 2000.f;
constexpr float kItemMinOpacity = 0.4f;

// The scale factor used to calculate the minimum side length for the overview
// item bounds on the desks bar.
constexpr float kScaleFactorForMinimumSideLength = 0.5f;

// The minimum vertical overlapped length between the overview item and new desk
// button in order to activate the new desk button.
constexpr int kVerticalOverlappedLengthToActivateNewDeskButton = 15;

// Amount of time we wait to unpause the occlusion tracker after a overview item
// is finished dragging. Waits a bit longer than the overview item animation.
constexpr base::TimeDelta kOcclusionPauseDurationForDrag =
    base::Milliseconds(300);

constexpr base::TimeDelta kScaleUpNewDeskButtonGracePeriod =
    base::Milliseconds(500);

// The UMA histogram that records presentation time for window dragging
// operation in overview mode.
constexpr char kOverviewWindowDragHistogram[] =
    "Ash.Overview.WindowDrag.PresentationTime.TabletMode";
constexpr char kOverviewWindowDragMaxLatencyHistogram[] =
    "Ash.Overview.WindowDrag.PresentationTime.MaxLatency.TabletMode";

bool g_skip_new_desk_button_scale_up_for_test = false;

bool GetVirtualDesksBarEnabled(OverviewItemBase* item) {
  return desks_util::ShouldDesksBarBeCreated() &&
         item->overview_grid()->desks_bar_view();
}

// Returns whether |item|'s window is visible on all desks.
bool DraggedItemIsVisibleOnAllDesks(OverviewItemBase* item) {
  aura::Window* const dragged_window = item->GetWindow();
  return dragged_window &&
         desks_util::IsWindowVisibleOnAllWorkspaces(dragged_window);
}

// Returns the scaled-down size of the dragged item that should be used when
// it's dragged over the OverviewDeskBarView that belongs to |overview_grid|.
// |window_original_size| is the size of the item's window before it was scaled
// up for dragging.
gfx::SizeF GetItemSizeWhenOnDesksBar(OverviewGrid* overview_grid,
                                     const gfx::SizeF& window_original_size) {
  DCHECK(overview_grid);
  const OverviewDeskBarView* desks_bar_view = overview_grid->desks_bar_view();
  DCHECK(desks_bar_view);

  const int expanded_desks_bar_height = DeskBarViewBase::GetPreferredBarHeight(
      overview_grid->root_window(), DeskBarViewBase::Type::kOverview,
      DeskBarViewBase::State::kExpanded);

  // We should always use the expanded desks bar height here even if the desks
  // bar is actually in zero state to calculate `scale_factor`. Because if zero
  // state bar height is used here, the dragged window could become too small
  // during the drag.
  const float scale_factor = static_cast<float>(expanded_desks_bar_height) /
                             overview_grid->root_window()->bounds().height();
  gfx::SizeF scaled_size = gfx::ScaleSize(window_original_size, scale_factor);

  // Adjust the scaled size to ensure that its smaller side length is equal or
  // larger than the `minimum_size_length`, and then adjust the larger size
  // length to preserve the ratio of the original size.
  const float minimum_size_length =
      expanded_desks_bar_height * kScaleFactorForMinimumSideLength;
  const float scaled_size_height = scaled_size.height();
  const float scaled_size_width = scaled_size.width();
  if (scaled_size_height < minimum_size_length ||
      scaled_size_width < minimum_size_length) {
    if (scaled_size_height < scaled_size_width) {
      scaled_size.set_height(minimum_size_length);
      scaled_size.set_width(scaled_size_width / scaled_size_height *
                            minimum_size_length);
    } else {
      scaled_size.set_width(minimum_size_length);
      scaled_size.set_height(scaled_size_height / scaled_size_width *
                             minimum_size_length);
    }
  }

  // Add the margins overview mode adds around the window's contents.
  scaled_size.Enlarge(kDraggingEnlargeDp,
                      kDraggingEnlargeDp + kWindowMiniViewHeaderHeight);
  return scaled_size;
}

float GetManhattanDistanceX(float point_x, const gfx::RectF& rect) {
  return std::max(rect.x() - point_x, point_x - rect.right());
}

float GetManhattanDistanceY(float point_y, const gfx::RectF& rect) {
  return std::max(rect.y() - point_y, point_y - rect.bottom());
}

void RecordDrag(OverviewDragAction action) {
  base::UmaHistogramEnumeration("Ash.Overview.WindowDrag.Workflow", action);
}

// Restores the new desk button state back to the
// `DeskIconButton::State::kExpanded` on drag ended on all `OverviewGrid`s.
void MaybeRestoreNewDeskButtonState() {
  OverviewSession* overview_session =
      OverviewController::Get()->overview_session();
  if (!overview_session || overview_session->is_shutting_down()) {
    return;
  }

  for (aura::Window* root : Shell::GetAllRootWindows()) {
    OverviewGrid* overview_grid = overview_session->GetGridWithRootWindow(root);
    if (auto* desks_bar_view = overview_grid->desks_bar_view()) {
      desks_bar_view->UpdateDeskIconButtonState(
          desks_bar_view->new_desk_button(), DeskIconButton::State::kExpanded);
    }
  }
}

// Helps with handling the workflow where you drag an overview item from one
// grid and drop into another grid. The challenge is that if the item represents
// an ARC window, that window will be moved to the target root asynchronously.
// |OverviewItemMoveHelper| observes the window until it moves to the target
// root. Then |OverviewItemMoveHelper| self destructs and adds a new item to
// represent the window on the target root.
class OverviewItemMoveHelper : public aura::WindowObserver {
 public:
  // |target_item_bounds| is the bounds of the dragged overview item when the
  // drag ends. |target_item_bounds| is used to put the new item where the old
  // item ended, so it looks like it is the same item. Then the item is animated
  // from there to its proper position in the grid.
  OverviewItemMoveHelper(aura::Window* window,
                         const gfx::RectF& target_item_bounds)
      : window_(window), target_item_bounds_(target_item_bounds) {
    window->AddObserver(this);
  }
  OverviewItemMoveHelper(const OverviewItemMoveHelper&) = delete;
  OverviewItemMoveHelper& operator=(const OverviewItemMoveHelper&) = delete;
  ~OverviewItemMoveHelper() override {
    OverviewController* overview_controller = OverviewController::Get();
    if (overview_controller->InOverviewSession()) {
      overview_controller->overview_session()->PositionWindows(
          /*animate=*/true);
    }
  }

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override {
    DCHECK_EQ(window_, window);
    delete this;
  }
  void OnWindowAddedToRootWindow(aura::Window* window) override {
    DCHECK_EQ(window_, window);
    window->RemoveObserver(this);
    OverviewController* overview_controller = OverviewController::Get();
    if (overview_controller->InOverviewSession()) {
      // OverviewSession::AddItemInMruOrder() will add |window| to the grid
      // associated with |window|'s root. Do not reposition or restack as we
      // will soon handle them both anyway.
      OverviewSession* session = overview_controller->overview_session();
      session->AddItemInMruOrder(window, /*reposition=*/false,
                                 /*animate=*/false, /*restack=*/false,
                                 /*use_spawn_animation=*/false);
      OverviewItemBase* item = session->GetOverviewItemForWindow(window);
      DCHECK(item);
      item->SetBounds(target_item_bounds_, OVERVIEW_ANIMATION_NONE);
      item->set_should_restack_on_animation_end(true);
      // The destructor will call OverviewSession::PositionWindows().
    }
    delete this;
  }

 private:
  const raw_ptr<aura::Window> window_;
  const gfx::RectF target_item_bounds_;
};

}  // namespace

OverviewWindowDragController::OverviewWindowDragController(
    OverviewSession* overview_session,
    OverviewItemBase* item,
    bool is_touch_dragging,
    OverviewItemBase* event_source_item)
    : overview_session_(overview_session),
      item_(item),
      event_source_item_(event_source_item),
      display_count_(Shell::GetAllRootWindows().size()),
      is_touch_dragging_(is_touch_dragging),
      is_eligible_for_drag_to_snap_(
          IsEligibleForDraggingToSnapInOverview(item)),
      virtual_desks_bar_enabled_(GetVirtualDesksBarEnabled(item)) {
  CHECK(!OverviewController::Get()->IsInStartAnimation());
  CHECK(!SplitViewController::Get(item_->root_window())->IsDividerAnimating());
}

OverviewWindowDragController::~OverviewWindowDragController() {
  // This object is deleted using `DeleteSoon()`, so the shell may be destroyed
  // already during shutdown.
  if (Shell::HasInstance()) {
    Shell::Get()->mouse_cursor_filter()->HideSharedEdgeIndicator();
  }
}

// static
base::AutoReset<bool>
OverviewWindowDragController::SkipNewDeskButtonScaleUpDurationForTesting() {
  return {&g_skip_new_desk_button_scale_up_for_test, true};
}

void OverviewWindowDragController::InitiateDrag(
    const gfx::PointF& location_in_screen) {
  initial_event_location_ = location_in_screen;
  initial_centerpoint_ = item_->target_bounds().CenterPoint();
  original_opacity_ = item_->GetOpacity();
  current_drag_behavior_ = DragBehavior::kUndefined;
  occlusion_pauser_ = OverviewController::Get()->PauseOcclusionTracker(
      kOcclusionPauseDurationForDrag);
  DCHECK(!presentation_time_recorder_);

  presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
      item_->root_window()->layer()->GetCompositor(),
      kOverviewWindowDragHistogram, kOverviewWindowDragMaxLatencyHistogram);
}

void OverviewWindowDragController::Drag(const gfx::PointF& location_in_screen) {
  if (!did_move_) {
    gfx::Vector2dF distance = location_in_screen - initial_event_location_;
    // Do not start dragging if the distance from |location_in_screen| to
    // |initial_event_location_| is not greater than |kMinimumDragDistanceDp|.
    if (std::abs(distance.x()) < kMinimumDragDistanceDp &&
        std::abs(distance.y()) < kMinimumDragDistanceDp) {
      return;
    }

    if (is_touch_dragging_ && std::abs(distance.x()) < std::abs(distance.y())) {
      StartDragToCloseMode();
    } else if (is_eligible_for_drag_to_snap_ || virtual_desks_bar_enabled_) {
      StartNormalDragMode(location_in_screen);
    } else {
      return;
    }
  }

  if (current_drag_behavior_ == DragBehavior::kDragToClose)
    ContinueDragToClose(location_in_screen);
  else if (current_drag_behavior_ == DragBehavior::kNormalDrag)
    ContinueNormalDrag(location_in_screen);

  if (presentation_time_recorder_)
    presentation_time_recorder_->RequestNext();
}

OverviewWindowDragController::DragResult
OverviewWindowDragController::CompleteDrag(
    const gfx::PointF& location_in_screen) {
  per_grid_desks_bar_data_.clear();
  DragResult result = DragResult::kNeverDisambiguated;

  switch (current_drag_behavior_) {
    case DragBehavior::kNoDrag:
      NOTREACHED();

    case DragBehavior::kUndefined:
      ActivateDraggedWindow();
      break;

    case DragBehavior::kNormalDrag:
      result = CompleteNormalDrag(location_in_screen);
      break;

    case DragBehavior::kDragToClose:
      result = CompleteDragToClose(location_in_screen);
      break;
  }

  did_move_ = false;

  // `item_` may be null if `CompleteNormalDrag()` resulted in moving the
  // window into another desk. At this point, we can just pass in a nullptr and
  // the `FloatContainerStacker` will reset the stacking. Also,
  // `ActivateDraggedWindow()` above may have started the session shutdown, so
  // the `FloatContainerStacker` may be null.
  if (auto* float_container_stacker =
          overview_session_->float_container_stacker()) {
    float_container_stacker->OnDragFinished(item_ ? item_->GetWindow()
                                                  : nullptr);
  }

  item_ = nullptr;
  event_source_item_ = nullptr;
  current_drag_behavior_ = DragBehavior::kNoDrag;
  occlusion_pauser_.reset();
  presentation_time_recorder_.reset();
  return result;
}

void OverviewWindowDragController::StartNormalDragMode(
    const gfx::PointF& location_in_screen) {
  CHECK(is_eligible_for_drag_to_snap_ || virtual_desks_bar_enabled_);

  did_move_ = true;
  current_drag_behavior_ = DragBehavior::kNormalDrag;
  Shell::Get()->mouse_cursor_filter()->ShowSharedEdgeIndicator(
      item_->root_window());
  const gfx::SizeF window_original_size(item_->GetWindow()->bounds().size());
  item_->ScaleUpSelectedItem(
      OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW);
  original_scaled_size_ = item_->target_bounds().size();
  auto* overview_grid = item_->overview_grid();
  overview_grid->AddDropTargetForDraggingFromThisGrid(item_);

  // Expand all desks bars on all displays when normal drag starts if it is in
  // zero state.
  for (const std::unique_ptr<OverviewGrid>& grid :
       overview_session_->grid_list()) {
    // The bar may be null if we have no desks in tablet mode.
    if (auto* desks_bar_view = grid->desks_bar_view();
        desks_bar_view && desks_bar_view->IsZeroState()) {
      desks_bar_view->UpdateNewMiniViews(/*initializing_bar_view=*/false,
                                         /*expanding_bar_view=*/true);
    }
  }

  item_->UpdateShadowTypeForDrag(/*is_dragging=*/true);
  aura::Window* dragged_window = item_->GetWindow();

  if (is_eligible_for_drag_to_snap_) {
    overview_session_->SetSplitViewDragIndicatorsDraggedWindow(dragged_window);
    overview_session_->UpdateSplitViewDragIndicatorsWindowDraggingStates(
        GetRootWindowBeingDraggedIn(),
        SplitViewDragIndicators::ComputeWindowDraggingState(
            /*is_dragging=*/true,
            SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            SnapPosition::kNone));
    item_->HideCannotSnapWarning(/*animate=*/true);

    // Update the split view divider bar status if necessary. If splitview is
    // active when dragging the `dragged_window`, the split divider bar should
    // be placed below the dragged window during dragging.
    SplitViewController::Get(item_->root_window())
        ->OnWindowDragStarted(dragged_window);
  }

  if (virtual_desks_bar_enabled_) {
    // Calculate the item bounds minus the header and margins (which are
    // invisible). Use this for the shrink bounds so that the item starts
    // shrinking when the visible top-edge of the item aligns with the
    // bottom-edge of the desks bar (may be different edges if we are dragging
    // from different directions).
    gfx::SizeF item_no_header_size = original_scaled_size_;
    item_no_header_size.Enlarge(
        float{-kDraggingEnlargeDp},
        float{-kDraggingEnlargeDp - kWindowMiniViewHeaderHeight});

    // We must update the desks bar widget bounds before we cache its bounds
    // below, in case it needs to be pushed down due to splitview indicators.
    // Note that when drag is just getting started, the window hasn't moved to
    // another display, so it's ok to use the item's |overview_grid|.
    overview_grid->MaybeUpdateDesksWidgetBounds();

    // Calculate cached values for usage during drag for each grid.
    for (const auto& grid : overview_session_->grid_list()) {
      GridDesksBarData& grid_desks_bar_data =
          per_grid_desks_bar_data_[grid.get()];

      grid_desks_bar_data.on_desks_bar_item_size =
          GetItemSizeWhenOnDesksBar(grid.get(), window_original_size);
      grid_desks_bar_data.desks_bar_bounds = grid_desks_bar_data.shrink_bounds =
          gfx::RectF(grid->desks_bar_view()->GetBoundsInScreen());
      const int expanded_height = DeskBarViewBase::GetPreferredBarHeight(
          grid->root_window(), DeskBarViewBase::Type::kOverview,
          DeskBarViewBase::State::kExpanded);
      grid_desks_bar_data.desks_bar_bounds.set_height(expanded_height);
      grid_desks_bar_data.shrink_bounds.set_height(expanded_height);
      grid_desks_bar_data.shrink_bounds.Inset(gfx::InsetsF::VH(
          -item_no_header_size.height() / 2, -item_no_header_size.width() / 2));
      grid_desks_bar_data.shrink_region_distance =
          grid_desks_bar_data.desks_bar_bounds.origin() -
          grid_desks_bar_data.shrink_bounds.origin();
    }
  }

  overview_session_->float_container_stacker()->OnDragStarted(dragged_window);
}

OverviewWindowDragController::DragResult OverviewWindowDragController::Fling(
    const gfx::PointF& location_in_screen,
    float velocity_x,
    float velocity_y) {
  if (current_drag_behavior_ == DragBehavior::kDragToClose ||
      current_drag_behavior_ == DragBehavior::kUndefined) {
    if (std::abs(velocity_y) > kFlingToCloseVelocityThreshold) {
      item_->AnimateAndCloseItem(
          (location_in_screen - initial_event_location_).y() < 0);
      did_move_ = false;
      item_ = nullptr;
      event_source_item_ = nullptr;
      current_drag_behavior_ = DragBehavior::kNoDrag;
      occlusion_pauser_.reset();
      RecordDragToClose(kFlingToClose);
      return DragResult::kSuccessfulDragToClose;
    }
  }

  // If the fling velocity was not high enough, or flings should be ignored,
  // treat it as a scroll end event.
  return CompleteDrag(location_in_screen);
}

void OverviewWindowDragController::ActivateDraggedWindow() {
  // If no drag was initiated (e.g., a click/tap on the overview window),
  // activate the window. If the split view is active and has a left window,
  // snap the current window to right. If the split view is active and has a
  // right window, snap the current window to left. If split view is active
  // and the selected window cannot be snapped, exit splitview and activate
  // the selected window, and also exit the overview.
  SplitViewController* split_view_controller =
      SplitViewController::Get(item_->root_window());
  SplitViewController::State split_state = split_view_controller->state();
  if (!is_eligible_for_drag_to_snap_ ||
      split_state == SplitViewController::State::kNoSnap) {
    overview_session_->SelectWindow(event_source_item_);
    // Explicitly set `item_` to null to avoid being accessed after been
    // released in `OverviewGrid::RemoveItem()`. See UaF reported in
    // b/301368132.
    item_ = nullptr;
    event_source_item_ = nullptr;
  } else if (auto* split_view_overview_session =
                 RootWindowController::ForWindow(item_->GetWindow())
                     ->split_view_overview_session();
             split_view_overview_session) {
    // If `SplitViewOverviewSession` is active, activate the window;
    // `AutoSnapController` will handle the autosnap.
    RecordPartialOverviewMetrics(item_);
    overview_session_->SelectWindow(event_source_item_);
    item_ = nullptr;
    event_source_item_ = nullptr;
  } else if (split_view_controller->CanSnapWindow(
                 item_->GetWindow(), chromeos::kDefaultSnapRatio)) {
    // Used for overview items that are being dragged to snap. Since the
    // window is already activated, `AutoSnapController::OnWindowActivating()`
    // will not work above.
    RecordPartialOverviewMetrics(item_);
    SnapWindow(split_view_controller,
               split_state == SplitViewController::State::kPrimarySnapped
                   ? SnapPosition::kSecondary
                   : SnapPosition::kPrimary);
  } else {
    split_view_controller->EndSplitView();
    overview_session_->SelectWindow(event_source_item_);
    // Same as above, explicitly set `item_` to nullptr to avoid UaF.
    item_ = nullptr;
    event_source_item_ = nullptr;
    ShowAppCannotSnapToast();
  }

  current_drag_behavior_ = DragBehavior::kNoDrag;
  occlusion_pauser_.reset();
}

void OverviewWindowDragController::ResetGesture() {
  if (current_drag_behavior_ == DragBehavior::kNormalDrag) {
    CHECK(item_->overview_grid()->drop_target());

    Shell::Get()->mouse_cursor_filter()->HideSharedEdgeIndicator();
    item_->DestroyMirrorsForDragging();
    overview_session_->RemoveDropTargets();
    if (is_eligible_for_drag_to_snap_) {
      SplitViewController::Get(item_->root_window())->OnWindowDragCanceled();
      overview_session_->ResetSplitViewDragIndicatorsWindowDraggingStates();
      item_->UpdateCannotSnapWarningVisibility(/*animate=*/true);
    }
  }

  // No need to position windows that are being destroyed.
  base::flat_set<OverviewItemBase*> ignored_items;
  if (item_->GetWindow()->is_destroying()) {
    ignored_items.insert(item_);
  }
  overview_session_->PositionWindows(/*animate=*/true, ignored_items);
  overview_session_->float_container_stacker()->OnDragFinished(
      item_->GetWindow());
  // This function gets called after a long press release, which bypasses
  // CompleteDrag but stops dragging as well, so reset |item_|.
  item_ = nullptr;
  event_source_item_ = nullptr;
  current_drag_behavior_ = DragBehavior::kNoDrag;
  occlusion_pauser_.reset();
}

void OverviewWindowDragController::ResetOverviewSession() {
  overview_session_ = nullptr;
  new_desk_button_scale_up_timer_.Stop();
}

void OverviewWindowDragController::StartDragToCloseMode() {
  DCHECK(is_touch_dragging_);

  did_move_ = true;
  current_drag_behavior_ = DragBehavior::kDragToClose;
  overview_session_->GetGridWithRootWindow(item_->root_window())
      ->StartNudge(item_);

  item_->UpdateShadowTypeForDrag(/*is_dragging=*/true);
  overview_session_->float_container_stacker()->OnDragStarted(
      item_->GetWindow());
}

void OverviewWindowDragController::ContinueDragToClose(
    const gfx::PointF& location_in_screen) {
  DCHECK_EQ(current_drag_behavior_, DragBehavior::kDragToClose);

  // Update the dragged |item_|'s bounds accordingly. The distance from the new
  // location to the new centerpoint should be the same it was initially.
  gfx::RectF bounds(item_->target_bounds());
  const gfx::PointF centerpoint =
      location_in_screen - (initial_event_location_ - initial_centerpoint_);

  // If the drag location intersects with the desk bar, then we should cancel
  // the drag-to-close mode and start the normal drag mode.
  if (virtual_desks_bar_enabled_ &&
      item_->overview_grid()->IntersectsWithDesksBar(
          gfx::ToRoundedPoint(location_in_screen),
          /*update_desks_bar_drag_details=*/false, /*for_drop=*/false)) {
    item_->SetOpacity(original_opacity_);
    StartNormalDragMode(location_in_screen);
    ContinueNormalDrag(location_in_screen);
    return;
  }

  // Update |item_|'s opacity based on its distance. |item_|'s x coordinate
  // should not change while in drag to close state.
  float val = std::abs(location_in_screen.y() - initial_event_location_.y()) /
              kDragToCloseDistanceThresholdDp;
  overview_session_->GetGridWithRootWindow(item_->root_window())
      ->UpdateNudge(item_, val);
  val = std::clamp(val, 0.f, 1.f);
  float opacity = original_opacity_;
  if (opacity > kItemMinOpacity)
    opacity = original_opacity_ - val * (original_opacity_ - kItemMinOpacity);

  item_->SetOpacity(opacity);

  // When dragging to close, only update the y component.
  bounds.set_y(centerpoint.y() - bounds.height() / 2.f);
  item_->SetBounds(bounds, OVERVIEW_ANIMATION_NONE);
}

OverviewWindowDragController::DragResult
OverviewWindowDragController::CompleteDragToClose(
    const gfx::PointF& location_in_screen) {
  DCHECK_EQ(current_drag_behavior_, DragBehavior::kDragToClose);

  // Close the window if it has been dragged enough, otherwise reposition it and
  // set its opacity back to its original value.
  overview_session_->GetGridWithRootWindow(item_->root_window())->EndNudge();
  const float y_distance = (location_in_screen - initial_event_location_).y();
  if (std::abs(y_distance) > kDragToCloseDistanceThresholdDp) {
    item_->AnimateAndCloseItem(/*up=*/y_distance < 0);
    RecordDragToClose(kSwipeToCloseSuccessful);
    return DragResult::kSuccessfulDragToClose;
  }

  item_->UpdateShadowTypeForDrag(/*is_dragging=*/false);

  item_->SetOpacity(original_opacity_);
  overview_session_->PositionWindows(/*animate=*/true);
  RecordDragToClose(kSwipeToCloseCanceled);
  return DragResult::kCanceledDragToClose;
}

void OverviewWindowDragController::ContinueNormalDrag(
    const gfx::PointF& location_in_screen) {
  DCHECK_EQ(current_drag_behavior_, DragBehavior::kNormalDrag);

  // Update the dragged |item_|'s bounds accordingly. The distance from the new
  // location to the new centerpoint should be the same it was initially unless
  // the item is over the DeskBarView, in which case we scale it down and center
  // it around the drag location.
  gfx::RectF bounds(item_->target_bounds());
  gfx::PointF centerpoint =
      location_in_screen - (initial_event_location_ - initial_centerpoint_);

  auto* overview_grid = GetCurrentGrid();

  // If virtual desks is enabled, we want to gradually shrink the dragged item
  // as it gets closer to get dropped into a desk mini view.
  if (virtual_desks_bar_enabled_) {
    // TODO(sammiequon): There is a slight jump especially if we drag from the
    // corner of a larger overview item, but this is necessary for the time
    // being to prevent jumps from happening while shrinking. Investigate if we
    // can satisfy all cases.
    centerpoint = location_in_screen;

    const auto iter = per_grid_desks_bar_data_.find(overview_grid);
    DCHECK(iter != per_grid_desks_bar_data_.end());
    const GridDesksBarData& desks_bar_data = iter->second;

    if (desks_bar_data.shrink_bounds.Contains(location_in_screen)) {
      // Update the mini views borders by checking if |location_in_screen|
      // intersects. Only update the borders if the dragged item is not visible
      // on all desks.
      overview_grid->IntersectsWithDesksBar(
          gfx::ToRoundedPoint(location_in_screen),
          /*update_desks_bar_drag_details=*/
          !DraggedItemIsVisibleOnAllDesks(item_), /*for_drop=*/false);

      float value = 0.f;
      if (centerpoint.y() < desks_bar_data.desks_bar_bounds.y() ||
          centerpoint.y() > desks_bar_data.desks_bar_bounds.bottom()) {
        // Coming vertically, this is the main use case. This is a ratio of the
        // distance from |centerpoint| to the closest edge of |desk_bar_bounds|
        // to the distance from |shrink_bounds| to |desk_bar_bounds|.
        value = GetManhattanDistanceY(centerpoint.y(),
                                      desks_bar_data.desks_bar_bounds) /
                desks_bar_data.shrink_region_distance.y();
      } else if (centerpoint.x() < desks_bar_data.desks_bar_bounds.x() ||
                 centerpoint.x() > desks_bar_data.desks_bar_bounds.right()) {
        // Coming horizontally, this only happens if we are in landscape split
        // view and someone drags an item to the other half, then up, then into
        // the desks bar. Works same as vertically except using x-coordinates.
        value = GetManhattanDistanceX(centerpoint.x(),
                                      desks_bar_data.desks_bar_bounds) /
                desks_bar_data.shrink_region_distance.x();
      }
      value = std::clamp(value, 0.f, 1.f);
      const gfx::SizeF size_value =
          gfx::Tween::SizeFValueBetween(1.f - value, original_scaled_size_,
                                        desks_bar_data.on_desks_bar_item_size);
      bounds.set_size(size_value);
    } else {
      bounds.set_size(original_scaled_size_);
    }
  }

  if (is_eligible_for_drag_to_snap_) {
    UpdateDragIndicatorsAndOverviewGrid(location_in_screen);
    // The newly updated indicator state may cause the desks widget to be pushed
    // down to make room for the top splitview guidance indicator when in
    // portrait orientation in tablet mode.
    overview_grid->MaybeUpdateDesksWidgetBounds();
  }

  if (!overview_grid->drop_target() &&
      (!is_eligible_for_drag_to_snap_ ||
       SplitViewDragIndicators::GetSnapPosition(
           overview_grid->split_view_drag_indicators()
               ->current_window_dragging_state()) == SnapPosition::kNone)) {
    overview_grid->AddDropTargetNotForDraggingFromThisGrid(item_->GetWindow(),
                                                           /*animate=*/true);
  }
  overview_session_->UpdateDropTargetsBackgroundVisibilities(
      item_, location_in_screen);

  bounds.set_x(centerpoint.x() - bounds.width() / 2.f);
  bounds.set_y(centerpoint.y() - bounds.height() / 2.f);
  item_->SetBounds(bounds, OVERVIEW_ANIMATION_NONE);

  // The bar may be null if we have no desks in tablet mode.
  if (auto* desks_bar_view = overview_grid->desks_bar_view()) {
    auto* new_desk_button = desks_bar_view->new_desk_button();

    // The header of window is shown during dragging. Overview item should be
    // hovered on the new desk button with
    // `kVerticalOverlappedLengthToActivateNewDeskButton` overlapped vertical
    // area in order to activate the new desk button. There could be a lot of
    // mistriggers with header shown if the new desk button is activated when
    // the overview item intersects with it.
    gfx::Rect effective_hovered_bounds(gfx::ToEnclosedRect(bounds));
    effective_hovered_bounds.Inset(gfx::Insets::TLBR(
        kVerticalOverlappedLengthToActivateNewDeskButton, 0, 0, 0));
    const bool is_hovered_on_new_desk_button =
        new_desk_button->GetBoundsInScreen().Intersects(
            effective_hovered_bounds);

    if (!is_hovered_on_new_desk_button) {
      new_desk_button_scale_up_timer_.Stop();
    } else if (!new_desk_button_scale_up_timer_.IsRunning() &&
               new_desk_button->state() == DeskIconButton::State::kExpanded) {
      if (g_skip_new_desk_button_scale_up_for_test) {
        MaybeScaleUpNewDeskButton();
      } else {
        new_desk_button_scale_up_timer_.Start(
            FROM_HERE, kScaleUpNewDeskButtonGracePeriod, this,
            &OverviewWindowDragController::MaybeScaleUpNewDeskButton);
      }
    }
  }

  if (display_count_ > 1u)
    item_->UpdateMirrorsForDragging(is_touch_dragging_);
}

OverviewWindowDragController::DragResult
OverviewWindowDragController::CompleteNormalDrag(
    const gfx::PointF& location_in_screen) {
  DCHECK_EQ(current_drag_behavior_, DragBehavior::kNormalDrag);
  auto* item_overview_grid = item_->overview_grid();
  CHECK(item_overview_grid->drop_target());
  Shell::Get()->mouse_cursor_filter()->HideSharedEdgeIndicator();
  item_->DestroyMirrorsForDragging();
  overview_session_->RemoveDropTargets();

  item_->UpdateShadowTypeForDrag(/*is_dragging=*/false);

  const gfx::Point rounded_screen_point =
      gfx::ToRoundedPoint(location_in_screen);
  if (is_eligible_for_drag_to_snap_) {
    // Update the split view divider bar status if necessary. The divider bar
    // should be placed above the dragged window after drag ends. Note here the
    // passed parameters |snap_position_| and |location_in_screen| won't be used
    // in this function for this case, but they are passed in as placeholders.
    aura::Window* window = item_->GetWindow();
    SplitViewController::Get(item_->root_window())
        ->OnWindowDragEnded(
            window, snap_position_, rounded_screen_point,
            WindowSnapActionSource::kDragOrSelectOverviewWindowToSnap);

    // Update window grid bounds and |snap_position_| in case the screen
    // orientation was changed.
    UpdateDragIndicatorsAndOverviewGrid(location_in_screen);
    overview_session_->ResetSplitViewDragIndicatorsWindowDraggingStates();
    item_->UpdateCannotSnapWarningVisibility(/*animate=*/true);
  }

  // This function has multiple exit positions, at each we must update the desks
  // bar widget bounds. We can't do this before we attempt dropping the window
  // on a desk mini_view, since this will change where it is relative to the
  // current |location_in_screen|.
  absl::Cleanup at_exit_runner = [] {
    // Overview might have exited if we snapped windows on both sides.
    auto* overview_controller = OverviewController::Get();
    if (!overview_controller->InOverviewSession())
      return;

    for (auto& grid : overview_controller->overview_session()->grid_list())
      grid->MaybeUpdateDesksWidgetBounds();
  };

  aura::Window* target_root = GetRootWindowBeingDraggedIn();
  const bool is_dragged_to_other_display = target_root != item_->root_window();
  auto* current_grid = GetCurrentGrid();
  if (virtual_desks_bar_enabled_) {
    item_->SetOpacity(original_opacity_);

    // Attempt to move a window to a different desk.
    if (current_grid->MaybeDropItemOnDeskMiniViewOrNewDeskButton(
            rounded_screen_point, item_)) {
      // Window was successfully moved to another desk, and |item_| was
      // removed from the grid. It may never be accessed after this.
      item_ = nullptr;
      event_source_item_ = nullptr;
      overview_session_->PositionWindows(/*animate=*/true);
      RecordNormalDrag(kToDesk, is_dragged_to_other_display);
      return DragResult::kDragToDesk;
    }
  }

  // Snap a window if appropriate.
  if (is_eligible_for_drag_to_snap_ && snap_position_ != SnapPosition::kNone) {
    // Overview grid will be updated after window is snapped in splitview.
    SnapWindow(SplitViewController::Get(target_root), snap_position_);
    RecordNormalDrag(kToSnap, is_dragged_to_other_display);
    MaybeRestoreNewDeskButtonState();
    return DragResult::kSnap;
  }

  DCHECK(item_);
  const bool dragged_item_is_visible_on_all_desks =
      DraggedItemIsVisibleOnAllDesks(item_);
  const bool item_intersects_other_display_desk_bar =
      virtual_desks_bar_enabled_ &&
      current_grid->IntersectsWithDesksBar(
          gfx::ToRoundedPoint(location_in_screen),
          /*update_desks_bar_drag_details=*/false, /*for_drop=*/false);

  // Drop a window into overview because we have not done anything else with it.
  // If the window is visible on all desks, only move it to another display if
  // it doesn't intersect the other grid's desk bar.
  if (is_dragged_to_other_display &&
      !(dragged_item_is_visible_on_all_desks &&
        item_intersects_other_display_desk_bar)) {
    // Get the window and bounds from |item_| before removing it from its grid.
    aura::Window* window = item_->GetWindow();
    const gfx::RectF target_item_bounds = item_->target_bounds();
    // Remove |item_| from overview. Leave the repositioning to the
    // |OverviewItemMoveHelper|.
    overview_session_->RemoveItem(item_, /*item_destroying=*/false,
                                  /*reposition=*/false);
    item_ = nullptr;
    event_source_item_ = nullptr;
    // The |OverviewItemMoveHelper| will self destruct when we move |window| to
    // |target_root|.
    new OverviewItemMoveHelper(window, target_item_bounds);
    // Move |window| to |target_root|. The |OverviewItemMoveHelper| will take
    // care of the rest.
    window_util::MoveWindowToDisplay(window,
                                     display::Screen::GetScreen()
                                         ->GetDisplayNearestWindow(target_root)
                                         .id());
  } else {
    item_->set_should_restack_on_animation_end(true);
    overview_session_->PositionWindows(/*animate=*/true);
    MaybeRestoreNewDeskButtonState();
  }
  RecordNormalDrag(kToGrid, is_dragged_to_other_display);
  return DragResult::kDropIntoOverview;
}

void OverviewWindowDragController::UpdateDragIndicatorsAndOverviewGrid(
    const gfx::PointF& location_in_screen) {
  CHECK(is_eligible_for_drag_to_snap_);
  snap_position_ = GetSnapPosition(location_in_screen);
  overview_session_->UpdateSplitViewDragIndicatorsWindowDraggingStates(
      GetRootWindowBeingDraggedIn(),
      SplitViewDragIndicators::ComputeWindowDraggingState(
          /*is_dragging=*/true,
          SplitViewDragIndicators::WindowDraggingState::kFromOverview,
          snap_position_));
  overview_session_->RearrangeDuringDrag(item_);
}

aura::Window* OverviewWindowDragController::GetRootWindowBeingDraggedIn()
    const {
  if (is_touch_dragging_) {
    return item_->root_window();
  }

  auto* screen = display::Screen::GetScreen();
  CHECK(screen);
  auto display = screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint());
  return Shell::GetRootWindowForDisplayId(display.id());
}

SnapPosition OverviewWindowDragController::GetSnapPosition(
    const gfx::PointF& location_in_screen) const {
  CHECK(item_);
  CHECK(is_eligible_for_drag_to_snap_);
  gfx::Rect area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          GetRootWindowBeingDraggedIn());

  // If split view mode is active at the moment, and dragging an overview window
  // to snap it to a position that already has a snapped window in place, we
  // should show the preview window as soon as the window past the split divider
  // bar.
  aura::Window* root_window = GetRootWindowBeingDraggedIn();
  SplitViewController* split_view_controller =
      SplitViewController::Get(root_window);
  if (!split_view_controller->CanSnapWindow(item_->GetWindow(),
                                            chromeos::kDefaultSnapRatio)) {
    return SnapPosition::kNone;
  }
  if (split_view_controller->InSplitViewMode()) {
    // If we're trying to snap to a position that already has a snapped window:
    aura::Window* default_snapped_window =
        split_view_controller->GetDefaultSnappedWindow();
    if (gfx::RectF(default_snapped_window->GetBoundsInScreen())
            .Contains(location_in_screen)) {
      return split_view_controller->GetPositionOfSnappedWindow(
          default_snapped_window);
    }
  }

  return ::ash::GetSnapPosition(
      root_window, item_->GetWindow(), gfx::ToRoundedPoint(location_in_screen),
      gfx::ToRoundedPoint(initial_event_location_),
      /*snap_distance_from_edge=*/kDistanceFromEdgeDp,
      /*minimum_drag_distance=*/kMinimumDragToSnapDistanceDp,
      /*horizontal_edge_inset=*/area.width() *
              kHighlightScreenPrimaryAxisRatio +
          kHighlightScreenEdgePaddingDp,
      /*vertical_edge_inset=*/area.height() * kHighlightScreenPrimaryAxisRatio +
          kHighlightScreenEdgePaddingDp);
}

void OverviewWindowDragController::SnapWindow(
    SplitViewController* split_view_controller,
    SnapPosition snap_position) {
  DCHECK_NE(snap_position, SnapPosition::kNone);

  CHECK(!SplitViewController::Get(item_->root_window())->IsDividerAnimating());
  aura::Window* window = item_->GetWindow();

  // If `window` is currently fullscreen, snapping it will trigger a work area
  // change, which triggers `OverviewSession::OnDisplayMetricsChanged`. Display
  // changes normally end dragging for simplicity, but we need `item` to be
  // nullptr before that happens so we can skip resetting the window gesture.
  // See crbug.com/1330042 for more details. `item_` will be deleted after
  // SplitViewController::SnapWindow().
  item_ = nullptr;
  event_source_item_ = nullptr;
  split_view_controller->SnapWindow(
      window, snap_position,
      WindowSnapActionSource::kDragOrSelectOverviewWindowToSnap,
      /*activate_window=*/true);
}

OverviewGrid* OverviewWindowDragController::GetCurrentGrid() const {
  return overview_session_->GetGridWithRootWindow(
      GetRootWindowBeingDraggedIn());
}

void OverviewWindowDragController::RecordNormalDrag(
    NormalDragAction action,
    bool is_dragged_to_other_display) const {
  const bool is_tablet = display::Screen::GetScreen()->InTabletMode();
  if (is_dragged_to_other_display) {
    DCHECK(!is_touch_dragging_);
    if (!is_tablet) {
      constexpr OverviewDragAction kDrag[kNormalDragActionEnumSize] = {
          OverviewDragAction::kToGridOtherDisplayClamshellMouse,
          OverviewDragAction::kToDeskOtherDisplayClamshellMouse,
          OverviewDragAction::kToSnapOtherDisplayClamshellMouse};
      RecordDrag(kDrag[action]);
    }
  } else if (is_tablet) {
    if (is_touch_dragging_) {
      constexpr OverviewDragAction kDrag[kNormalDragActionEnumSize] = {
          OverviewDragAction::kToGridSameDisplayTabletTouch,
          OverviewDragAction::kToDeskSameDisplayTabletTouch,
          OverviewDragAction::kToSnapSameDisplayTabletTouch};
      RecordDrag(kDrag[action]);
    }
  } else {
    constexpr OverviewDragAction kMouseDrag[kNormalDragActionEnumSize] = {
        OverviewDragAction::kToGridSameDisplayClamshellMouse,
        OverviewDragAction::kToDeskSameDisplayClamshellMouse,
        OverviewDragAction::kToSnapSameDisplayClamshellMouse};
    constexpr OverviewDragAction kTouchDrag[kNormalDragActionEnumSize] = {
        OverviewDragAction::kToGridSameDisplayClamshellTouch,
        OverviewDragAction::kToDeskSameDisplayClamshellTouch,
        OverviewDragAction::kToSnapSameDisplayClamshellTouch};
    RecordDrag(is_touch_dragging_ ? kTouchDrag[action] : kMouseDrag[action]);
  }
}

void OverviewWindowDragController::RecordDragToClose(
    DragToCloseAction action) const {
  DCHECK(is_touch_dragging_);
  constexpr OverviewDragAction kClamshellDrag[kDragToCloseActionEnumSize] = {
      OverviewDragAction::kSwipeToCloseSuccessfulClamshellTouch,
      OverviewDragAction::kSwipeToCloseCanceledClamshellTouch,
      OverviewDragAction::kFlingToCloseClamshellTouch};
  constexpr OverviewDragAction kTabletDrag[kDragToCloseActionEnumSize] = {
      OverviewDragAction::kSwipeToCloseSuccessfulTabletTouch,
      OverviewDragAction::kSwipeToCloseCanceledTabletTouch,
      OverviewDragAction::kFlingToCloseTabletTouch};
  RecordDrag(display::Screen::GetScreen()->InTabletMode()
                 ? kTabletDrag[action]
                 : kClamshellDrag[action]);
}

void OverviewWindowDragController::MaybeScaleUpNewDeskButton() {
  if (!item_ || !item_->overview_grid()) {
    return;
  }

  // When there's only one window and it's snapped, overview mode will be
  // ended. Thus we need to check whether `overview_session_` is being
  // shutting down or not here before triggering `UpdateDeskIconButtonState`.
  if (!overview_session_) {
    return;
  }

  auto* overview_grid =
      overview_session_->GetGridWithRootWindow(GetRootWindowBeingDraggedIn());
  auto* desks_bar_view = overview_grid->desks_bar_view();
  auto* new_desk_button = desks_bar_view->new_desk_button();

  if (!new_desk_button->GetEnabled()) {
    return;
  }

  // Do not reposition the windows while changing the desk icon button. This
  // could cause items to shift around mid drag.
  overview_session_->SuspendReposition();
  desks_bar_view->UpdateDeskIconButtonState(
      new_desk_button, /*target_state=*/DeskIconButton::State::kActive);
  overview_session_->ResumeReposition();
}

}  // namespace ash

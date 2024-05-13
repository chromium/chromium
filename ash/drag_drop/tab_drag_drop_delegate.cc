// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/tab_drag_drop_delegate.h"

#include "ash/constants/ash_features.h"
#include "ash/drag_drop/tab_drag_drop_windows_hider.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_metrics.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/crosapi/cpp/lacros_startup_state.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The following distances are copied from tablet_mode_window_drag_delegate.cc.
// TODO(crbug.com/40126106): share these constants.

// Items dragged to within |kDistanceFromEdgeDp| of the screen will get snapped
// even if they have not moved by |kMinimumDragToSnapDistanceDp|.
constexpr float kDistanceFromEdgeDp = 16.f;
// The minimum distance that an item must be moved before it is snapped. This
// prevents accidental snaps.
constexpr float kMinimumDragToSnapDistanceDp = 96.f;

// The scale factor that the source window should scale if the source window is
// not the dragged window && is not in splitscreen when drag starts && the user
// has dragged the window to pass the |kIndicatorThresholdRatio| vertical
// threshold.
constexpr float kSourceWindowScale = 0.85;

// The UMA histogram that records presentation time for tab dragging in
// tablet mode with webui tab strip enable.
constexpr char kTabDraggingInTabletModeHistogram[] =
    "Ash.TabDrag.PresentationTime.TabletMode";

constexpr char kTabDraggingInTabletModeMaxLatencyHistogram[] =
    "Ash.TabDrag.PresentationTime.MaxLatency.TabletMode";

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsSourceWindowForDrag, false)

bool IsLacrosWindow(const aura::Window* window) {
  auto app_type = window->GetProperty(chromeos::kAppTypeKey);
  return app_type == chromeos::AppType::LACROS;
}

// Returns the overview session if overview mode is active, otherwise returns
// nullptr.
OverviewSession* GetOverviewSession() {
  return Shell::Get()->overview_controller()->InOverviewSession()
             ? Shell::Get()->overview_controller()->overview_session()
             : nullptr;
}

}  // namespace

// static
bool TabDragDropDelegate::IsChromeTabDrag(const ui::OSExchangeData& drag_data) {
  return Shell::Get()->shell_delegate()->IsTabDrag(drag_data);
}

// static
bool TabDragDropDelegate::IsSourceWindowForDrag(const aura::Window* window) {
  return window->GetProperty(kIsSourceWindowForDrag);
}

TabDragDropDelegate::TabDragDropDelegate(
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& start_location_in_screen)
    : root_window_(root_window),
      source_window_(source_window->GetToplevelWindow()),
      start_location_in_screen_(start_location_in_screen) {
  DCHECK(root_window_);
  DCHECK(source_window_);
  source_window_->AddObserver(this);
  source_window_->SetProperty(kIsSourceWindowForDrag, true);
  split_view_drag_indicators_ =
      std::make_unique<SplitViewDragIndicators>(root_window_);

  tab_dragging_recorder_ = CreatePresentationTimeHistogramRecorder(
      source_window_->layer()->GetCompositor(),
      kTabDraggingInTabletModeHistogram,
      kTabDraggingInTabletModeMaxLatencyHistogram);
}

TabDragDropDelegate::~TabDragDropDelegate() {
  tab_dragging_recorder_.reset();

  if (!source_window_) {
    return;
  }

  source_window_->RemoveObserver(this);

  if (source_window_->is_destroying())
    return;

  if (!source_window_->GetProperty(kIsSourceWindowForDrag))
    return;

  // If we didn't drop to a new window, we must restore the original window.
  RestoreSourceWindowBounds();
  source_window_->ClearProperty(kIsSourceWindowForDrag);
}

void TabDragDropDelegate::DragUpdate(const gfx::Point& location_in_screen) {
  const gfx::Rect area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);

  SnapPosition snap_position = ash::GetSnapPositionForLocation(
      Shell::GetPrimaryRootWindow(), location_in_screen,
      start_location_in_screen_,
      /*snap_distance_from_edge=*/kDistanceFromEdgeDp,
      /*minimum_drag_distance=*/kMinimumDragToSnapDistanceDp,
      /*horizontal_edge_inset=*/area.width() *
              kHighlightScreenPrimaryAxisRatio +
          kHighlightScreenEdgePaddingDp,
      /*vertical_edge_inset=*/area.height() * kHighlightScreenPrimaryAxisRatio +
          kHighlightScreenEdgePaddingDp);
  if (ShouldPreventSnapToTheEdge(location_in_screen))
    snap_position = SnapPosition::kNone;

  split_view_drag_indicators_->SetWindowDraggingState(
      SplitViewDragIndicators::ComputeWindowDraggingState(
          true, SplitViewDragIndicators::WindowDraggingState::kFromTop,
          snap_position));

  UpdateSourceWindowBoundsIfNecessary(snap_position, location_in_screen);

  tab_dragging_recorder_->RequestNext();
}

void TabDragDropDelegate::DropAndDeleteSelf(
    const gfx::Point& location_in_screen,
    const ui::OSExchangeData& drop_data) {
  tab_dragging_recorder_.reset();

  // Release input capture in advance.
  ReleaseCapture();

  auto closure = base::BindOnce(&TabDragDropDelegate::OnNewBrowserWindowCreated,
                                base::Owned(this), location_in_screen);
  NewWindowDelegate::GetPrimary()->NewWindowForDetachingTab(
      source_window_, drop_data, std::move(closure));
}

void TabDragDropDelegate::OnWindowDestroying(aura::Window* window) {
  if (source_window_ == window) {
    windows_hider_.reset();
    source_window_->RemoveObserver(this);
    source_window_ = nullptr;
  }
}

void TabDragDropDelegate::OnNewBrowserWindowCreated(
    const gfx::Point& location_in_screen,
    aura::Window* new_window) {
  // `source_window_` could reset to nullptr during the drag.
  if (!source_window_) {
    DCHECK(!new_window);
    return;
  }

  auto is_lacros = IsLacrosWindow(source_window_);

  // https://crbug.com/1286203:
  // It's possible new window is created when the dragged WebContents
  // closes itself during the drag session.
  if (!new_window) {
    if (is_lacros && !crosapi::lacros_startup_state::IsLacrosEnabled()) {
      LOG(ERROR) << "New browser window creation for tab detaching failed.\n"
                 << "Check whether Lacros is enabled";
    }
    return;
  }

  const gfx::Rect area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);

  SnapPosition snap_position_in_snapping_zone = ash::GetSnapPosition(
      root_window_, new_window, location_in_screen, start_location_in_screen_,
      /*snap_distance_from_edge=*/kDistanceFromEdgeDp,
      /*minimum_drag_distance=*/kMinimumDragToSnapDistanceDp,
      /*horizontal_edge_inset=*/area.width() *
              kHighlightScreenPrimaryAxisRatio +
          kHighlightScreenEdgePaddingDp,
      /*vertical_edge_inset=*/area.height() * kHighlightScreenPrimaryAxisRatio +
          kHighlightScreenEdgePaddingDp);
  if (ShouldPreventSnapToTheEdge(location_in_screen))
    snap_position_in_snapping_zone = SnapPosition::kNone;

  if (snap_position_in_snapping_zone == SnapPosition::kNone) {
    RestoreSourceWindowBounds();
  }

  // This must be done after restoring the source window's bounds since
  // otherwise the SetBounds() call may have no effect.
  source_window_->ClearProperty(kIsSourceWindowForDrag);

  SplitViewController* const split_view_controller =
      SplitViewController::Get(new_window);

  // If it's already in split view mode, either snap the new window
  // to the left or the right depending on the drop location.
  const bool in_split_view_mode = split_view_controller->InSplitViewMode();
  SnapPosition snap_position = snap_position_in_snapping_zone;
  if (in_split_view_mode) {
    snap_position =
        split_view_controller->ComputeSnapPosition(location_in_screen);
  }

  if (snap_position == SnapPosition::kNone) {
    return;
  }

  OverviewSession* overview_session = GetOverviewSession();
  // If overview session is present on the other side and the new window is
  // about to snap to that side but not in the snapping zone then drop the new
  // window into overview.
  if (overview_session &&
      snap_position_in_snapping_zone == SnapPosition::kNone &&
      split_view_controller->GetPositionOfSnappedWindow(source_window_) !=
          snap_position) {
    overview_session->MergeWindowIntoOverviewForWebUITabStrip(new_window);
  } else {
    split_view_controller->SnapWindow(new_window, snap_position,
                                      WindowSnapActionSource::kDragTabToSnap,
                                      /*activate_window=*/true);
  }

  // Do not snap the source window if already in split view mode.
  if (in_split_view_mode)
    return;

  // The tab drag source window is the last window the user was
  // interacting with. When dropping into split view, it makes the most
  // sense to snap this window to the opposite side. Do this.
  SnapPosition opposite_position = (snap_position == SnapPosition::kPrimary)
                                       ? SnapPosition::kSecondary
                                       : SnapPosition::kPrimary;

  // |source_window_| is itself a child window of the browser since it
  // hosts web content (specifically, the tab strip WebUI). Snap its
  // toplevel window which is the browser window.
  split_view_controller->SnapWindow(source_window_, opposite_position,
                                    WindowSnapActionSource::kDragTabToSnap);
}

bool TabDragDropDelegate::ShouldPreventSnapToTheEdge(
    const gfx::Point& location_in_screen) {
  SplitViewController* const split_view_controller =
      SplitViewController::Get(source_window_);
  return !split_view_controller->InSplitViewMode() &&
         IsLayoutHorizontal(source_window_) &&
         location_in_screen.y() <
             Shell::Get()->shell_delegate()->GetBrowserWebUITabStripHeight();
}

void TabDragDropDelegate::UpdateSourceWindowBoundsIfNecessary(
    SnapPosition candidate_snap_position,
    const gfx::Point& location_in_screen) {
  SplitViewController* const split_view_controller =
      SplitViewController::Get(source_window_);

  if (split_view_controller->IsWindowInSplitView(source_window_))
    return;

  if (!windows_hider_) {
    windows_hider_ = std::make_unique<TabDragDropWindowsHider>(source_window_);
  }

  gfx::Rect new_source_window_bounds;
  if (candidate_snap_position == SnapPosition::kNone) {
    const gfx::Rect area =
        screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
            root_window_);
    new_source_window_bounds = area;

    // Only shrink the window when the tab is dragged out of WebUI tab strip.
    if (location_in_screen.y() >
        Shell::Get()->shell_delegate()->GetBrowserWebUITabStripHeight()) {
      new_source_window_bounds.ClampToCenteredSize(
          gfx::Size(area.width() * kSourceWindowScale,
                    area.height() * kSourceWindowScale));
    }
  } else {
    const SnapPosition opposite_position =
        (candidate_snap_position == SnapPosition::kPrimary)
            ? SnapPosition::kSecondary
            : SnapPosition::kPrimary;
    new_source_window_bounds =
        SplitViewController::Get(source_window_)
            ->GetSnappedWindowBoundsInScreen(
                opposite_position, source_window_,
                window_util::GetSnapRatioForWindow(source_window_),
                /*account_for_divider_width=*/
                display::Screen::GetScreen()->InTabletMode());
  }
  wm::ConvertRectFromScreen(source_window_->parent(),
                            &new_source_window_bounds);

  if (new_source_window_bounds != source_window_->GetTargetBounds()) {
    ui::ScopedLayerAnimationSettings settings(
        source_window_->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    source_window_->SetBounds(new_source_window_bounds);
  }
}

void TabDragDropDelegate::RestoreSourceWindowBounds() {
  if (SplitViewController::Get(source_window_)
          ->IsWindowInSplitView(source_window_)) {
    return;
  }

  auto* window_state = WindowState::Get(source_window_);
  if (window_state->IsFloated()) {
    // This will notify `FloatController` to find the ideal floated window
    // bounds in tablet mode.
    TabletModeWindowState::UpdateWindowPosition(
        window_state, WindowState::BoundsChangeAnimationType::kNone);
    return;
  }

  const gfx::Rect area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window_);

  source_window_->SetBounds(area);
}

}  // namespace ash

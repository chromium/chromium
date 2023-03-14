// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_session.h"

#include <functional>
#include <utility>

#include "ash/accelerators/debug_commands.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/utility/haptics_util.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/templates/saved_desk_dialog_controller.h"
#include "ash/wm/desks/templates/saved_desk_grid_view.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "ash/wm/desks/templates/saved_desk_library_view.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_delegate.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/scoped_float_container_stacker.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Values for scrolling the grid by using the keyboard.
// TODO(sammiequon): See if we can use the same values used for web scrolling.
constexpr int kKeyboardPressScrollingDp = 75;
constexpr int kKeyboardHoldScrollingDp = 15;

// Tries to end overview. Returns true if overview is successfully ended, or
// just was not active in the first place.
bool EndOverview(OverviewEndAction action) {
  return Shell::Get()->overview_controller()->EndOverview(action);
}

// A self-deleting window state observer that runs the given callback when its
// associated window state has been changed.
class AsyncWindowStateChangeObserver : public WindowStateObserver,
                                       public aura::WindowObserver {
 public:
  AsyncWindowStateChangeObserver(
      aura::Window* window,
      base::OnceCallback<void(WindowState*)> on_post_window_state_changed)
      : window_(window),
        on_post_window_state_changed_(std::move(on_post_window_state_changed)) {
    DCHECK(!on_post_window_state_changed_.is_null());
    WindowState::Get(window_)->AddObserver(this);
    window_->AddObserver(this);
  }

  ~AsyncWindowStateChangeObserver() override { RemoveAllObservers(); }

  AsyncWindowStateChangeObserver(const AsyncWindowStateChangeObserver&) =
      delete;
  AsyncWindowStateChangeObserver& operator=(
      const AsyncWindowStateChangeObserver&) = delete;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override { delete this; }

  // WindowStateObserver:
  void OnPostWindowStateTypeChange(WindowState* window_state,
                                   chromeos::WindowStateType) override {
    RemoveAllObservers();
    std::move(on_post_window_state_changed_).Run(window_state);
    delete this;
  }

 private:
  void RemoveAllObservers() {
    WindowState::Get(window_)->RemoveObserver(this);
    window_->RemoveObserver(this);
  }

  aura::Window* window_;

  base::OnceCallback<void(WindowState*)> on_post_window_state_changed_;
};

// Simple override of views::Button. Allows to use a element of accessibility
// role button as the overview focus widget's contents.
class OverviewFocusButton : public views::Button {
 public:
  OverviewFocusButton() : views::Button(views::Button::PressedCallback()) {
    // Make this not focusable to avoid accessibility error since this view has
    // no accessible name.
    SetFocusBehavior(FocusBehavior::NEVER);
  }
  OverviewFocusButton(const OverviewFocusButton&) = delete;
  OverviewFocusButton& operator=(const OverviewFocusButton&) = delete;
  ~OverviewFocusButton() override = default;
};

}  // namespace

OverviewSession::OverviewSession(OverviewDelegate* delegate)
    : delegate_(delegate),
      overview_start_time_(base::Time::Now()),
      highlight_controller_(
          std::make_unique<OverviewHighlightController>(this)),
      chromevox_enabled_(Shell::Get()
                             ->accessibility_controller()
                             ->spoken_feedback()
                             .enabled()) {
  DCHECK(delegate_);
  Shell::Get()->AddPreTargetHandler(this);
}

OverviewSession::~OverviewSession() {
  // Don't delete |window_drag_controller_| yet since the stack might be still
  // using it.
  if (window_drag_controller_) {
    window_drag_controller_->ResetOverviewSession();
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, window_drag_controller_.release());
  }
}

// NOTE: The work done in Init() is not done in the constructor because it may
// cause other, unrelated classes, to make indirect method calls on a partially
// constructed object.
void OverviewSession::Init(const WindowList& windows,
                           const WindowList& hide_windows) {
  Shell::Get()->AddShellObserver(this);

  if (saved_desk_util::IsSavedDesksEnabled()) {
    tablet_mode_observation_.Observe(Shell::Get()->tablet_mode_controller());
    hide_windows_for_saved_desks_grid_ =
        std::make_unique<ScopedOverviewHideWindows>(
            /*windows=*/std::vector<aura::Window*>({}), /*forced_hidden=*/true);
  }

  hide_overview_windows_ = std::make_unique<ScopedOverviewHideWindows>(
      std::move(hide_windows), /*force_hidden=*/false);

  active_window_before_overview_ = window_util::GetActiveWindow();
  if (active_window_before_overview_) {
    active_window_before_overview_observation_.Observe(
        active_window_before_overview_);
  }

  // Create this before the desks bar widget.
  if (saved_desk_util::IsSavedDesksEnabled() && !saved_desk_presenter_) {
    saved_desk_presenter_ = std::make_unique<SavedDeskPresenter>(this);
    saved_desk_dialog_controller_ =
        std::make_unique<SavedDeskDialogController>();
  }

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::sort(root_windows.begin(), root_windows.end(),
            [](const aura::Window* a, const aura::Window* b) {
              // Since we don't know if windows are vertically or horizontally
              // oriented we use both x and y position. This may be confusing
              // if you have 3 or more monitors which are not strictly
              // horizontal or vertical but that case is not yet supported.
              return (a->GetBoundsInScreen().x() + a->GetBoundsInScreen().y()) <
                     (b->GetBoundsInScreen().x() + b->GetBoundsInScreen().y());
            });

  for (auto* root : root_windows) {
    auto grid = std::make_unique<OverviewGrid>(root, windows, this);
    num_items_ += grid->size();
    grid_list_.push_back(std::move(grid));
  }

  // The calls to OverviewGrid::PrepareForOverview() requires some
  // LayoutManagers to perform layouts so that windows are correctly visible and
  // properly animated in overview mode. Otherwise these layouts should be
  // suppressed during overview mode so they don't conflict with overview mode
  // animations.

  // Do not call PrepareForOverview until all items are added to window_list_
  // as we don't want to cause any window updates until all windows in
  // overview are observed. See http://crbug.com/384495.
  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_) {
    overview_grid->PrepareForOverview();

    // Do not animate if there is any window that is being dragged in the
    // grid.
    if (ShouldEnterWithoutAnimations()) {
      overview_grid->PositionWindows(/*animate=*/false);
    } else {
      // Exit only types should not appear here:
      DCHECK_NE(enter_exit_overview_type_, OverviewEnterExitType::kFadeOutExit);

      overview_grid->PositionWindows(/*animate=*/true, /*ignored_items=*/{},
                                     OverviewTransition::kEnter);
    }
  }

  UpdateNoWindowsWidgetOnEachGrid();

  // Create the widget that will receive focus while in overview mode for
  // accessibility purposes. Add a button as the contents so that
  // `UpdateAccessibilityFocus` can put it on the accessibility focus
  // cycler.
  overview_focus_widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = false;
  params.bounds = gfx::Rect(0, 0, 2, 2);
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.name = "OverviewModeFocusWidget";
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.init_properties_container.SetProperty(ash::kExcludeInMruKey, true);
  overview_focus_widget_->Init(std::move(params));
  overview_focus_widget_->SetContentsView(
      std::make_unique<OverviewFocusButton>());

  UMA_HISTOGRAM_COUNTS_100("Ash.Overview.Items", num_items_);

  SplitViewController::Get(Shell::GetPrimaryRootWindow())->AddObserver(this);

  display_observer_.emplace(this);
  base::RecordAction(base::UserMetricsAction("WindowSelector_Overview"));
  // Send an a11y alert.
  Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
      AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED);

  desks_controller_observation_.Observe(DesksController::Get());

  ignore_activations_ = false;
}

// NOTE: The work done in `Shutdown()` is not done in the destructor because it
// may cause other, unrelated classes, to make indirect calls to
// `restoring_minimized_windows()` on a partially destructed object.
void OverviewSession::Shutdown() {
  bool was_saved_desk_library_showing = false;
  for (auto& grid : grid_list_) {
    if (grid->IsShowingSavedDeskLibrary()) {
      was_saved_desk_library_showing = true;
      break;
    }
  }

  // This should have been set already when the process of ending overview mode
  // began. See OverviewController::OnSelectionEnded().
  DCHECK(is_shutting_down_);

  desks_controller_observation_.Reset();
  if (observing_desk_) {
    for (auto* root : Shell::GetAllRootWindows())
      observing_desk_->GetDeskContainerForRoot(root)->RemoveObserver(this);
  }

  Shell::Get()->RemovePreTargetHandler(this);
  Shell::Get()->RemoveShellObserver(this);

  float_container_stacker_.reset();

  tablet_mode_observation_.Reset();

  // Stop the presenter from receiving any events that may update the model or
  // UI.
  saved_desk_presenter_.reset();

  // Resetting here will close any dialogs, and DCHECK anyone trying to open a
  // dialog past this point.
  saved_desk_dialog_controller_.reset();

  // Stop observing screen metrics changes first to avoid auto-positioning
  // windows in response to work area changes from window activation.
  display_observer_.reset();

  // Stop observing split view state changes before restoring window focus.
  // Otherwise the activation of the window triggers OnSplitViewStateChanged()
  // that will call into this function again.
  SplitViewController::Get(Shell::GetPrimaryRootWindow())->RemoveObserver(this);

  size_t remaining_items = 0;
  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_) {
    // During shutdown, do not animate all windows in overview if we need to
    // animate the snapped window.
    if (overview_grid->should_animate_when_exiting() &&
        enter_exit_overview_type_ != OverviewEnterExitType::kImmediateExit) {
      overview_grid->CalculateWindowListAnimationStates(
          selected_item_ &&
                  selected_item_->overview_grid() == overview_grid.get()
              ? selected_item_
              : nullptr,
          OverviewTransition::kExit, /*target_bounds=*/{});
    }
    for (const auto& overview_item : overview_grid->window_list()) {
      overview_item->RestoreWindow(/*reset_transform=*/true,
                                   was_saved_desk_library_showing);
    }
    remaining_items += overview_grid->size();
  }

  // Setting focus after restoring windows' state avoids unnecessary animations.
  // No need to restore if we are fading out to the home launcher screen, as all
  // windows will be minimized.
  const bool should_restore =
      enter_exit_overview_type_ == OverviewEnterExitType::kNormal ||
      enter_exit_overview_type_ == OverviewEnterExitType::kImmediateExit;
  RestoreWindowActivation(should_restore);
  RemoveAllObservers();

  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_)
    overview_grid->Shutdown(enter_exit_overview_type_);

  DCHECK(num_items_ >= remaining_items);
  if (!was_saved_desk_library_showing) {
    UMA_HISTOGRAM_COUNTS_100("Ash.Overview.OverviewClosedItems",
                             num_items_ - remaining_items);
    UMA_HISTOGRAM_MEDIUM_TIMES("Ash.Overview.TimeInOverview",
                               base::Time::Now() - overview_start_time_);
  }

  grid_list_.clear();

  // Hide the focus widget on overview session end to prevent it from retaining
  // focus and handling key press events now that overview session is not
  // consuming them.
  if (overview_focus_widget_)
    overview_focus_widget_->Hide();
}

void OverviewSession::OnGridEmpty() {
  if (!IsEmpty())
    return;

  if (SplitViewController::Get(Shell::GetPrimaryRootWindow())
          ->InTabletSplitViewMode()) {
    UpdateNoWindowsWidgetOnEachGrid();
  } else if (!allow_empty_desk_without_exiting_ &&
             !IsShowingSavedDeskLibrary()) {
    EndOverview(OverviewEndAction::kLastWindowRemoved);
  }
}

void OverviewSession::IncrementSelection(bool forward) {
  Move(/*reverse=*/!forward);
}

bool OverviewSession::AcceptSelection() {
  // Activate selected window or desk.
  return highlight_controller_->MaybeActivateHighlightedViewOnOverviewExit();
}

void OverviewSession::SelectWindow(OverviewItem* item) {
  aura::Window* window = item->GetWindow();
  aura::Window::Windows window_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  if (!window_list.empty()) {
    // Record WindowSelector_ActiveWindowChanged if the user is selecting a
    // window other than the window that was active prior to entering overview
    // mode (i.e., the window at the front of the MRU list).
    if (window_list[0] != window) {
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_ActiveWindowChanged"));
      Shell::Get()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(
          TaskSwitchSource::OVERVIEW_MODE);
    }
    const auto it = base::ranges::find(window_list, window);
    if (it != window_list.end()) {
      // Record 1-based index so that selecting a top MRU window will record 1.
      UMA_HISTOGRAM_COUNTS_100("Ash.Overview.SelectionDepth",
                               1 + it - window_list.begin());
    }
  }
  item->EnsureVisible();
  if (window->GetProperty(kPipOriginalWindowKey)) {
    window_util::ExpandArcPipWindow();
    return;
  }
  // If the selected window is a minimized window, un-minimize it first before
  // activating it so that the window can use the scale-up animation instead of
  // un-minimizing animation. The activation of the window will happen in an
  // asynchronous manner on window state has been changed. That's because some
  // windows (ARC app windows) have their window states changed async, so we
  // need to wait until the window is fully unminimized before activation as
  // opposed to having two consecutive calls.
  auto* window_state = WindowState::Get(window);
  if (window_state->IsMinimized()) {
    ScopedAnimationDisabler disabler(window);
    // The following instance self-destructs when the window state changed.
    new AsyncWindowStateChangeObserver(
        window, base::BindOnce([](WindowState* window_state) {
          for (auto* window_iter : window_util::GetVisibleTransientTreeIterator(
                   window_state->window())) {
            window_iter->layer()->SetOpacity(1.0);
          }
          wm::ActivateWindow(window_state->window());
        }));
    // If we are in split mode, use Show() here to delegate un-minimizing to
    // SplitViewController as it handles auto snapping cases.
    if (SplitViewController::Get(window)->InSplitViewMode())
      window->Show();
    else
      window_state->Unminimize();
    return;
  }

  wm::ActivateWindow(window);
}

void OverviewSession::SetSplitViewDragIndicatorsDraggedWindow(
    aura::Window* dragged_window) {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->SetSplitViewDragIndicatorsDraggedWindow(dragged_window);
}

void OverviewSession::UpdateSplitViewDragIndicatorsWindowDraggingStates(
    const aura::Window* root_window_being_dragged_in,
    SplitViewDragIndicators::WindowDraggingState
        state_on_root_window_being_dragged_in) {
  if (state_on_root_window_being_dragged_in ==
      SplitViewDragIndicators::WindowDraggingState::kNoDrag) {
    ResetSplitViewDragIndicatorsWindowDraggingStates();
    return;
  }
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    grid->SetSplitViewDragIndicatorsWindowDraggingState(
        grid->root_window() == root_window_being_dragged_in
            ? state_on_root_window_being_dragged_in
            : SplitViewDragIndicators::WindowDraggingState::kOtherDisplay);
  }
}

void OverviewSession::ResetSplitViewDragIndicatorsWindowDraggingStates() {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    grid->SetSplitViewDragIndicatorsWindowDraggingState(
        SplitViewDragIndicators::WindowDraggingState::kNoDrag);
  }
}

void OverviewSession::RearrangeDuringDrag(OverviewItem* dragged_item) {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    DCHECK(grid->split_view_drag_indicators());
    grid->RearrangeDuringDrag(
        dragged_item,
        grid->split_view_drag_indicators()->current_window_dragging_state());
  }
}

void OverviewSession::UpdateDropTargetsBackgroundVisibilities(
    OverviewItem* dragged_item,
    const gfx::PointF& location_in_screen) {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    if (grid->GetDropTarget()) {
      grid->UpdateDropTargetBackgroundVisibility(dragged_item,
                                                 location_in_screen);
    }
  }
}

OverviewGrid* OverviewSession::GetGridWithRootWindow(
    aura::Window* root_window) {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    if (grid->root_window() == root_window)
      return grid.get();
  }

  return nullptr;
}

void OverviewSession::AddItem(
    aura::Window* window,
    bool reposition,
    bool animate,
    const base::flat_set<OverviewItem*>& ignored_items,
    size_t index) {
  // Early exit if a grid already contains |window|.
  OverviewGrid* grid = GetGridWithRootWindow(window->GetRootWindow());
  if (!grid || grid->GetOverviewItemContaining(window))
    return;

  base::AutoReset<bool> ignore(&ignore_activations_, true);
  grid->AddItem(window, reposition, animate, ignored_items, index,
                /*use_spawn_animation=*/false, /*restack=*/false);
  OnItemAdded(window);
}

void OverviewSession::AppendItem(aura::Window* window,
                                 bool reposition,
                                 bool animate) {
  // Early exit if a grid already contains |window|.
  OverviewGrid* grid = GetGridWithRootWindow(window->GetRootWindow());
  if (!grid || grid->GetOverviewItemContaining(window))
    return;

  if (IsShowingSavedDeskLibrary())
    animate = false;

  base::AutoReset<bool> ignore(&ignore_activations_, true);
  grid->AppendItem(window, reposition, animate, /*use_spawn_animation=*/true);
  OnItemAdded(window);
}

void OverviewSession::AddItemInMruOrder(aura::Window* window,
                                        bool reposition,
                                        bool animate,
                                        bool restack,
                                        bool use_spawn_animation) {
  // Early exit if a grid already contains |window|.
  OverviewGrid* grid = GetGridWithRootWindow(window->GetRootWindow());
  if (!grid || grid->GetOverviewItemContaining(window))
    return;

  base::AutoReset<bool> ignore(&ignore_activations_, true);
  grid->AddItemInMruOrder(window, reposition, animate, restack,
                          use_spawn_animation);
  OnItemAdded(window);
}

void OverviewSession::RemoveItem(OverviewItem* overview_item) {
  RemoveItem(overview_item, /*item_destroying=*/false, /*reposition=*/false);
}

void OverviewSession::RemoveItem(OverviewItem* overview_item,
                                 bool item_destroying,
                                 bool reposition) {
  if (overview_item->GetWindow() == active_window_before_overview_) {
    active_window_before_overview_observation_.Reset();
    active_window_before_overview_ = nullptr;
  }

  overview_item->overview_grid()->RemoveItem(overview_item, item_destroying,
                                             reposition);
  --num_items_;

  UpdateNoWindowsWidgetOnEachGrid();
  UpdateAccessibilityFocus();
}

void OverviewSession::RemoveDropTargets() {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    if (grid->GetDropTarget())
      grid->RemoveDropTarget();
  }
}

void OverviewSession::InitiateDrag(OverviewItem* item,
                                   const gfx::PointF& location_in_screen,
                                   bool is_touch_dragging) {
  if (Shell::Get()->overview_controller()->IsInStartAnimation() ||
      SplitViewController::Get(Shell::GetPrimaryRootWindow())
          ->IsDividerAnimating()) {
    return;
  }

  highlight_controller_->SetFocusHighlightVisibility(false);
  window_drag_controller_ = std::make_unique<OverviewWindowDragController>(
      this, item, is_touch_dragging);
  window_drag_controller_->InitiateDrag(location_in_screen);

  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    grid->OnSelectorItemDragStarted(item);
    grid->UpdateSaveDeskButtons();
  }

  // Fire a haptic event if necessary.
  if (!is_touch_dragging) {
    haptics_util::PlayHapticTouchpadEffect(
        ui::HapticTouchpadEffect::kTick,
        ui::HapticTouchpadEffectStrength::kMedium);
  }
}

void OverviewSession::Drag(OverviewItem* item,
                           const gfx::PointF& location_in_screen) {
  DCHECK(window_drag_controller_);
  DCHECK_EQ(item, window_drag_controller_->item());
  window_drag_controller_->Drag(location_in_screen);
}

void OverviewSession::CompleteDrag(OverviewItem* item,
                                   const gfx::PointF& location_in_screen) {
  DCHECK(window_drag_controller_);
  DCHECK_EQ(item, window_drag_controller_->item());

  // Note: The highlight should be updated first as completing a drag may cause
  // a selection which would destroy |item|.
  highlight_controller_->SetFocusHighlightVisibility(true);
  const bool snap = window_drag_controller_->CompleteDrag(location_in_screen) ==
                    OverviewWindowDragController::DragResult::kSnap;
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    grid->OnSelectorItemDragEnded(snap);
    grid->UpdateSaveDeskButtons();
  }
}

void OverviewSession::StartNormalDragMode(
    const gfx::PointF& location_in_screen) {
  window_drag_controller_->StartNormalDragMode(location_in_screen);
}

void OverviewSession::Fling(OverviewItem* item,
                            const gfx::PointF& location_in_screen,
                            float velocity_x,
                            float velocity_y) {
  // Its possible a fling event is not paired with a tap down event. Ignore
  // these flings.
  if (!window_drag_controller_ || item != window_drag_controller_->item())
    return;

  const bool snap = window_drag_controller_->Fling(location_in_screen,
                                                   velocity_x, velocity_y) ==
                    OverviewWindowDragController::DragResult::kSnap;
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    grid->OnSelectorItemDragEnded(snap);
    grid->UpdateSaveDeskButtons();
  }
}

void OverviewSession::ActivateDraggedWindow() {
  window_drag_controller_->ActivateDraggedWindow();
}

void OverviewSession::ResetDraggedWindowGesture() {
  window_drag_controller_->ResetGesture();
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    grid->OnSelectorItemDragEnded(/*snap=*/false);
    grid->UpdateSaveDeskButtons();
  }
}

void OverviewSession::OnWindowDragStarted(aura::Window* dragged_window,
                                          bool animate) {
  OverviewGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragStarted(dragged_window, animate);

  // The stacker object may be already created depending on the overview enter
  // type.
  if (!float_container_stacker_) {
    float_container_stacker_ = std::make_unique<ScopedFloatContainerStacker>();
  }
  float_container_stacker_->OnDragStarted(dragged_window);
}

void OverviewSession::OnWindowDragContinued(
    aura::Window* dragged_window,
    const gfx::PointF& location_in_screen,
    SplitViewDragIndicators::WindowDraggingState window_dragging_state) {
  OverviewGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragContinued(dragged_window, location_in_screen,
                                     window_dragging_state);
}

void OverviewSession::OnWindowDragEnded(aura::Window* dragged_window,
                                        const gfx::PointF& location_in_screen,
                                        bool should_drop_window_into_overview,
                                        bool snap) {
  DCHECK(float_container_stacker_);
  float_container_stacker_->OnDragFinished(dragged_window);

  OverviewGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragEnded(dragged_window, location_in_screen,
                                 should_drop_window_into_overview, snap);
}

void OverviewSession::MergeWindowIntoOverviewForWebUITabStrip(
    aura::Window* dragged_window) {
  OverviewGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->MergeWindowIntoOverviewForWebUITabStrip(dragged_window);
}

void OverviewSession::SetVisibleDuringWindowDragging(bool visible,
                                                     bool animate) {
  for (auto& grid : grid_list_)
    grid->SetVisibleDuringWindowDragging(visible, animate);
}

void OverviewSession::PositionWindows(
    bool animate,
    const base::flat_set<OverviewItem*>& ignored_items) {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->PositionWindows(animate, ignored_items);

  RefreshNoWindowsWidgetBoundsOnEachGrid(animate);
}

bool OverviewSession::IsWindowInOverview(const aura::Window* window) {
  for (const std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    if (grid->GetOverviewItemContaining(window))
      return true;
  }
  return false;
}

OverviewItem* OverviewSession::GetOverviewItemForWindow(
    const aura::Window* window) {
  for (const std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    OverviewItem* item = grid->GetOverviewItemContaining(window);
    if (item)
      return item;
  }

  return nullptr;
}

void OverviewSession::SetWindowListNotAnimatedWhenExiting(
    aura::Window* root_window) {
  // Find the grid accociated with |root_window|.
  OverviewGrid* grid = GetGridWithRootWindow(root_window);
  if (grid)
    grid->SetWindowListNotAnimatedWhenExiting();
}

void OverviewSession::UpdateRoundedCornersAndShadow() {
  for (auto& grid : grid_list_)
    for (auto& window : grid->window_list())
      window->UpdateRoundedCornersAndShadow();
}

void OverviewSession::OnStartingAnimationComplete(bool canceled,
                                                  bool should_focus_overview) {
  for (auto& grid : grid_list_)
    grid->OnStartingAnimationComplete(canceled);

  if (canceled)
    return;

  if (overview_focus_widget_) {
    if (should_focus_overview) {
      overview_focus_widget_->Show();
    } else {
      overview_focus_widget_->ShowInactive();

      // Check if the active window is in overview. There is at least one
      // workflow where it will be: the active window is being dragged, and the
      // previous window carries over from clamshell mode to tablet split view.
      if (IsWindowInOverview(window_util::GetActiveWindow()) &&
          SplitViewController::Get(Shell::GetPrimaryRootWindow())
              ->InSplitViewMode()) {
        // We do not want an active window in overview. It will cause blatantly
        // broken behavior as in the video linked in crbug.com/992223.
        wm::ActivateWindow(
            SplitViewController::Get(Shell::GetPrimaryRootWindow())
                ->GetDefaultSnappedWindow());
      }
    }
  }

  UpdateAccessibilityFocus();
  Shell::Get()->overview_controller()->DelayedUpdateRoundedCornersAndShadow();

  // The stacker object may be already created if a drag has started prior to
  // this.
  if (!float_container_stacker_) {
    float_container_stacker_ = std::make_unique<ScopedFloatContainerStacker>();
  }
}

void OverviewSession::OnWindowActivating(
    ::wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (ignore_activations_ || gained_active == GetOverviewFocusWindow())
    return;

  // Activating the Desks bar window, the Saved Desk Library window, or the Save
  // Desk Button Container window should not end overview.
  if (gained_active &&
      (gained_active->GetId() == kShellWindowId_DesksBarWindow ||
       gained_active->GetId() == kShellWindowId_SavedDeskLibraryWindow ||
       gained_active->GetId() == kShellWindowId_SaveDeskButtonContainer)) {
    return;
  }

  // Activating or deactivating one of the confirmation dialogs associated with
  // saved desk should not end overview.
  if (gained_active && saved_desk_util::IsSavedDesksEnabled()) {
    if (ShouldKeepOverviewOpenForSavedDeskDialog(gained_active, lost_active))
      return;
  }

  if (DesksController::Get()->AreDesksBeingModified()) {
    // Activating a desk from its mini view will activate its most-recently used
    // window, but this should not result in ending overview mode now.
    // Overview will be ended explicitly as part of the desk activation
    // animation.
    return;
  }

  if (!gained_active) {
    // Cancel overview session and do not restore activation when active window
    // is set to nullptr. This happens when removing a display.
    RestoreWindowActivation(false);
    EndOverview(OverviewEndAction::kWindowActivating);
    return;
  }

  // The message center takes activation when someone clicks one of its buttons.
  // We shouldn't close overview in that case. There are two different possible
  // message center widgets. The stand alone one, and the one that is part of
  // the unified system tray bubble.
  if (gained_active->GetName() ==
      AshMessagePopupCollection::kMessagePopupWidgetName) {
    return;
  }

  for (RootWindowController* root_window_controller :
       Shell::GetAllRootWindowControllers()) {
    UnifiedSystemTray* system_tray =
        root_window_controller->GetStatusAreaWidget()->unified_system_tray();
    if (system_tray->IsMessageCenterBubbleShown()) {
      if (gained_active == system_tray->message_center_bubble()
                               ->GetBubbleWidget()
                               ->GetNativeWindow()) {
        return;
      }
    }
  }

  // If app list is open in clamshell mode, end overview. Note: we have special
  // logic to end overview when app list (i.e., home launcher) is open in tablet
  // mode, so do not handle it here.
  if (gained_active == Shell::Get()->app_list_controller()->GetWindow() &&
      !Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    RestoreWindowActivation(false);
    EndOverview(OverviewEndAction::kAppListActivatedInClamshell);
    return;
  }

  // Do not cancel overview mode if the window activation happens when split
  // view mode is also active. SplitViewController will do the right thing to
  // handle the window activation change. Check for split view mode without
  // using |SplitViewController::state_| which is updated asynchronously when
  // snapping an ARC window.
  // We also check if `gained_active` is to-be-snapped transitional state. In
  // the case, the window has not been attached to SplitViewController yet but
  // will be very soon.
  SplitViewController* split_view_controller =
      SplitViewController::Get(gained_active);
  if (split_view_controller->primary_window() ||
      split_view_controller->secondary_window() ||
      split_view_controller->IsWindowInTransitionalState(gained_active)) {
    RestoreWindowActivation(false);
    return;
  }

  // Do not cancel overview mode while a window or overview item is being
  // dragged as evidenced by the presence of a drop target. (Dragging to close
  // does not count; canceling overview mode is okay then.)
  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_) {
    if (overview_grid->GetDropTarget())
      return;
  }

  auto* grid = GetGridWithRootWindow(gained_active->GetRootWindow());
  DCHECK(grid);
  if (OverviewItem* item = grid->GetOverviewItemContaining(gained_active))
    selected_item_ = item;

  // Don't restore window activation on exit if a window was just activated.
  RestoreWindowActivation(false);
  EndOverview(OverviewEndAction::kWindowActivating);
}

bool OverviewSession::IsSavedDeskUiLosingActivation(aura::Window* lost_active) {
  if (!saved_desk_util::IsSavedDesksEnabled() || !lost_active)
    return false;

  for (auto& grid : grid_list_) {
    auto* desk_library_view = grid->GetSavedDeskLibraryView();
    if (desk_library_view &&
        lost_active == desk_library_view->GetWidget()->GetNativeWindow()) {
      return true;
    }
  }

  return saved_desk_dialog_controller_ &&
         saved_desk_dialog_controller_->dialog_widget() &&
         saved_desk_dialog_controller_->dialog_widget()->GetNativeWindow() ==
             lost_active;
}

aura::Window* OverviewSession::GetOverviewFocusWindow() {
  if (overview_focus_widget_)
    return overview_focus_widget_->GetNativeWindow();

  return nullptr;
}

aura::Window* OverviewSession::GetHighlightedWindow() {
  OverviewItem* item = highlight_controller_->GetHighlightedItem();
  if (!item)
    return nullptr;
  return item->GetWindow();
}

void OverviewSession::SuspendReposition() {
  for (auto& grid : grid_list_)
    grid->set_suspend_reposition(true);
}

void OverviewSession::ResumeReposition() {
  for (auto& grid : grid_list_)
    grid->set_suspend_reposition(false);
}

bool OverviewSession::IsEmpty() const {
  for (const auto& grid : grid_list_) {
    if (!grid->empty())
      return false;
  }
  return true;
}

void OverviewSession::RestoreWindowActivation(bool restore) {
  if (!active_window_before_overview_)
    return;

  // Do not restore focus to a window that exists on an inactive desk.
  restore &= base::Contains(DesksController::Get()->active_desk()->windows(),
                            active_window_before_overview_);

  // Ensure the window is still in the window hierarchy and not in the middle
  // of teardown.
  if (restore && active_window_before_overview_->GetRootWindow()) {
    base::AutoReset<bool> restoring_focus(&ignore_activations_, true);
    wm::ActivateWindow(active_window_before_overview_);
  }
  active_window_before_overview_observation_.Reset();
  active_window_before_overview_ = nullptr;
}

void OverviewSession::OnHighlightedItemActivated(OverviewItem* item) {
  UMA_HISTOGRAM_COUNTS_100("Ash.Overview.ArrowKeyPresses", num_key_presses_);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.Overview.KeyPressesOverItemsRatio",
                              (num_key_presses_ * 100) / num_items_, 1, 300,
                              30);
  base::RecordAction(
      base::UserMetricsAction("WindowSelector_OverviewEnterKey"));
  SelectWindow(item);
}

void OverviewSession::OnHighlightedItemClosed(OverviewItem* item) {
  base::RecordAction(
      base::UserMetricsAction("WindowSelector_OverviewCloseKey"));
  item->CloseWindow();
}

void OverviewSession::OnRootWindowClosing(aura::Window* root) {
  auto iter = base::ranges::find(grid_list_, root, &OverviewGrid::root_window);
  DCHECK(iter != grid_list_.end());
  (*iter)->Shutdown(OverviewEnterExitType::kImmediateExit);
  grid_list_.erase(iter);
}

OverviewItem* OverviewSession::GetCurrentDraggedOverviewItem() const {
  if (!window_drag_controller_)
    return nullptr;
  return window_drag_controller_->item();
}

bool OverviewSession::CanProcessEvent() const {
  return CanProcessEvent(/*sender=*/nullptr, /*from_touch_gesture=*/false);
}

bool OverviewSession::CanProcessEvent(OverviewItem* sender,
                                      bool from_touch_gesture) const {
  // Allow processing the event if no current window is being dragged.
  const bool drag_in_progress = window_util::IsAnyWindowDragged();
  if (!drag_in_progress)
    return true;

  // At this point, if there is no sender, we can't process the event since
  // |drag_in_progress| will be true.
  if (!sender || !window_drag_controller_)
    return false;

  // Allow processing the event if the sender is the one currently being
  // dragged and the event is the same type as the current one.
  if (sender == window_drag_controller_->item() &&
      from_touch_gesture == window_drag_controller_->is_touch_dragging()) {
    return true;
  }

  return false;
}

bool OverviewSession::IsWindowActiveWindowBeforeOverview(
    aura::Window* window) const {
  DCHECK(window);
  return window == active_window_before_overview_;
}

void OverviewSession::ShowSavedDeskLibrary(
    const base::GUID& item_to_focus,
    const std::u16string& saved_desk_name,
    aura::Window* const root_window) {
  if (Shell::Get()->tablet_mode_controller()->InTabletMode() ||
      IsShowingSavedDeskLibrary()) {
    return;
  }

  const bool created_grid_widgets =
      !grid_list_.front()->GetSavedDeskLibraryView();

  // Send an a11y alert.
  Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
      AccessibilityAlert::SAVED_DESKS_MODE_ENTERED);

  for (auto& grid : grid_list_)
    grid->ShowSavedDeskLibrary();

  // Only ask for all entries if it is the first time creating the grid widgets.
  // Otherwise, add or update the entries one at a time.
  if (created_grid_widgets) {
    saved_desk_presenter_->GetAllEntries(item_to_focus, saved_desk_name,
                                         root_window);
  }
  UpdateNoWindowsWidgetOnEachGrid();

  UpdateAccessibilityFocus();

  // TODO(crbug.com/1307467): This doesn't need to be reset if it's an ancestor
  // of the desks bar view. Also, add testing for this. Note that this isn't
  // needed when hiding, because we either move the focus to the new desk, or
  // delete all the grid templates items which would reset their highlights.
  highlight_controller_->ResetHighlightedView();

  // If not given anything to focus, focus the first saved desk.
  if (item_to_focus.is_valid())
    return;

  OverviewGrid* overview_grid = GetGridWithRootWindow(root_window);
  if (!overview_grid)
    return;

  SavedDeskLibraryView* library_view = overview_grid->GetSavedDeskLibraryView();
  if (!library_view)
    return;

  std::vector<SavedDeskGridView*> grid_views = library_view->grid_views();
  if (grid_views.empty())
    return;

  std::vector<SavedDeskItemView*> grid_items = grid_views.front()->grid_items();
  if (grid_items.empty() ||
      library_view->GetWidget()->GetNativeWindow()->GetRootWindow() !=
          root_window) {
    return;
  }

  highlight_controller_->MoveHighlightToView(
      grid_items.front(), /*suppress_accessibility_event=*/false);
}

void OverviewSession::HideSavedDeskLibrary() {
  // Before hiding the saved desk library, we need to explicitly activate the
  // focus window. Otherwise, some other window may get activated as the saved
  // desk library is hidden, and this could in turn lead to exiting overview
  // mode.
  wm::ActivateWindow(GetOverviewFocusWindow());

  for (auto& grid : grid_list_)
    grid->HideSavedDeskLibrary(/*exit_overview=*/false);

  UpdateAccessibilityFocus();
}

bool OverviewSession::IsShowingSavedDeskLibrary() const {
  // All the overview grids should show the saved desk grid at the same time so
  // just check if the first grid is showing.
  return grid_list_.empty() ? false
                            : grid_list_.front()->IsShowingSavedDeskLibrary();
}

bool OverviewSession::WillShowSavedDeskLibrary() const {
  return grid_list_.empty() ? false
                            : grid_list_.front()->WillShowSavedDeskLibrary();
}

bool OverviewSession::ShouldEnterWithoutAnimations() const {
  return enter_exit_overview_type_ == OverviewEnterExitType::kImmediateEnter ||
         enter_exit_overview_type_ ==
             OverviewEnterExitType::kImmediateEnterWithoutFocus;
}

void OverviewSession::UpdateAccessibilityFocus() {
  if (is_shutting_down())
    return;

  // Construct the list of accessible widgets, these are the overview focus
  // widget, desk bar widget, all the item widgets and the no window indicator
  // widgets, if available.
  std::vector<views::Widget*> a11y_widgets;
  if (overview_focus_widget_)
    a11y_widgets.push_back(overview_focus_widget_.get());

  // Note that this order matches the order of the tab cycling in
  // `OverviewHighlightController::GetTraversableViews`.
  for (auto& grid : grid_list_) {
    if (grid->IsShowingSavedDeskLibrary()) {
      a11y_widgets.push_back(grid->saved_desk_library_widget());
    } else {
      for (const auto& item : grid->window_list())
        a11y_widgets.push_back(item->item_widget());
    }
    if (grid->desks_widget())
      a11y_widgets.push_back(const_cast<views::Widget*>(grid->desks_widget()));

    if (grid->IsSaveDeskButtonContainerVisible())
      a11y_widgets.push_back(grid->save_desk_button_container_widget());

    auto* no_windows_widget = grid->no_windows_widget();
    if (no_windows_widget) {
      a11y_widgets.push_back(
          static_cast<views::Widget*>(grid->no_windows_widget()));
    }
  }

  if (a11y_widgets.empty())
    return;

  auto get_view_a11y = [&a11y_widgets](int index) -> views::ViewAccessibility& {
    return a11y_widgets[index]->GetContentsView()->GetViewAccessibility();
  };

  // If there is only one widget left, clear the focus overrides so that they
  // do not point to deleted objects.
  if (a11y_widgets.size() == 1) {
    get_view_a11y(/*index=*/0).OverridePreviousFocus(nullptr);
    get_view_a11y(/*index=*/0).OverrideNextFocus(nullptr);
    a11y_widgets[0]->GetContentsView()->NotifyAccessibilityEvent(
        ax::mojom::Event::kTreeChanged, true);
    return;
  }

  int size = a11y_widgets.size();
  for (int i = 0; i < size; ++i) {
    int previous_index = (i + size - 1) % size;
    int next_index = (i + 1) % size;
    get_view_a11y(i).OverridePreviousFocus(a11y_widgets[previous_index]);
    get_view_a11y(i).OverrideNextFocus(a11y_widgets[next_index]);
    a11y_widgets[i]->GetContentsView()->NotifyAccessibilityEvent(
        ax::mojom::Event::kTreeChanged, true);
  }
}

void OverviewSession::OnDeskActivationChanged(const Desk* activated,
                                              const Desk* deactivated) {
  observing_desk_ = activated;

  for (auto* root : Shell::GetAllRootWindows()) {
    activated->GetDeskContainerForRoot(root)->AddObserver(this);
    deactivated->GetDeskContainerForRoot(root)->RemoveObserver(this);

    if (auto* overview_grid = GetGridWithRootWindow(root))
      overview_grid->UpdateSaveDeskButtons();
  }
}

void OverviewSession::OnDisplayAdded(const display::Display& display) {
  if (EndOverview(OverviewEndAction::kDisplayAdded))
    return;
  SplitViewController::Get(Shell::GetPrimaryRootWindow())->EndSplitView();
  EndOverview(OverviewEndAction::kDisplayAdded);
}

void OverviewSession::OnDisplayMetricsChanged(const display::Display& display,
                                              uint32_t metrics) {
  // End the current drag if the display changes.
  if (window_drag_controller_ && window_drag_controller_->item())
    ResetDraggedWindowGesture();
  auto* overview_grid =
      GetGridWithRootWindow(Shell::GetRootWindowForDisplayId(display.id()));
  overview_grid->OnDisplayMetricsChanged();

  // In case of split view mode, the no windows widget bounds will be updated in
  // |OnSplitViewDividerPositionChanged|.
  if (SplitViewController::Get(Shell::GetPrimaryRootWindow())
          ->InSplitViewMode()) {
    return;
  }
  overview_grid->RefreshNoWindowsWidgetBounds(/*animate=*/false);
}

void OverviewSession::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(active_window_before_overview_, window);
  active_window_before_overview_observation_.Reset();
  active_window_before_overview_ = nullptr;
}

void OverviewSession::OnWindowAdded(aura::Window* new_window) {
  if (!auto_add_windows_enabled_)
    return;

  // We track if we are in the process of adding an item to avoid recursively
  // adding items.
  if (is_adding_new_item_)
    return;
  base::AutoReset<bool> adding_new_item_resetter(&is_adding_new_item_, true);

  // Avoid adding overview items for certain windows.
  if (!WindowState::Get(new_window) ||
      window_util::ShouldExcludeForOverview(new_window)) {
    return;
  }

  AddItemInMruOrder(new_window, /*reposition=*/true, /*animate=*/true,
                    /*restack=*/false, /*use_spawn_animation=*/true);

  // If a window is added from desk templates, we no longer want to go back to
  // the previous active window we had before entering overview, otherwise we
  // may activate a window and break the stacking order that the saved desk had.
  active_window_before_overview_observation_.Reset();
  active_window_before_overview_ = nullptr;
}

void OverviewSession::OnKeyEvent(ui::KeyEvent* event) {
  // If app list is open when overview is active (it can happen in clamshell
  // mode, when we snap an overview window to one side of the screen and then
  // open the app list to select an app to snap to the other side), in this case
  // we let the app list to handle the key event.
  // TODO(crbug.com/952315): Explore better ways to handle this splitview +
  // overview + applist case.
  Shell* shell = Shell::Get();
  if (!shell->tablet_mode_controller()->InTabletMode() &&
      shell->app_list_controller()->IsVisible()) {
    return;
  }

  // If a desk templates dialog is visible it should receive the key events.
  if (saved_desk_dialog_controller_ &&
      saved_desk_dialog_controller_->dialog_widget()) {
    return;
  }

  // If any name is being modified, let the name view handle the key events.
  // Note that Tab presses should commit any pending name changes.
  const ui::KeyboardCode key_code = event->key_code();
  const bool is_key_press = event->type() == ui::ET_KEY_PRESSED;
  const bool should_commit_name_changes =
      is_key_press && key_code == ui::VKEY_TAB;
  for (auto& grid : grid_list_) {
    if (grid->IsDeskNameBeingModified() ||
        grid->IsSavedDeskNameBeingModified()) {
      if (!should_commit_name_changes)
        return;

      // Commit and proceed.
      grid->CommitNameChanges();
      break;
    }
  }

  // Check if we can scroll with the event first as it can use release events as
  // well.
  if (ProcessForScrolling(*event)) {
    event->SetHandled();
    event->StopPropagation();
    return;
  }

  if (!is_key_press)
    return;

  const bool is_control_down = event->IsControlDown();

  switch (key_code) {
    case ui::VKEY_BROWSER_BACK:
    case ui::VKEY_ESCAPE:
      EndOverview(OverviewEndAction::kKeyEscapeOrBack);
      break;
    case ui::VKEY_UP:
      ++num_key_presses_;
      Move(/*reverse=*/true);
      break;
    case ui::VKEY_DOWN:
      ++num_key_presses_;
      Move(/*reverse=*/false);
      break;
    case ui::VKEY_RIGHT:
      ++num_key_presses_;
      if (!is_control_down ||
          !highlight_controller_->MaybeSwapHighlightedView(/*right=*/true)) {
        Move(/*reverse=*/false);
      }
      break;
    case ui::VKEY_TAB: {
      const bool reverse = event->IsShiftDown();
      ++num_key_presses_;
      Move(reverse);
      break;
    }
    case ui::VKEY_LEFT:
      ++num_key_presses_;
      if (!is_control_down ||
          !highlight_controller_->MaybeSwapHighlightedView(/*right=*/false)) {
        Move(/*reverse=*/true);
      }
      break;
    case ui::VKEY_W: {
      if (!is_control_down)
        return;

      const bool primary_action = !event->IsShiftDown();
      if (!highlight_controller_->MaybeCloseHighlightedView(primary_action))
        return;
      break;
    }
    case ui::VKEY_Z: {
      // Ctrl + Z undos a close all operation if the toast has not yet expired.
      // Ctrl + Alt + Z triggers ChromeVox so we don't do anything here to
      // interrupt that.
      if (!is_control_down || (is_control_down && event->IsAltDown())) {
        return;
      }

      DesksController::Get()->MaybeCancelDeskRemoval();
      break;
    }
    case ui::VKEY_RETURN: {
      if (!highlight_controller_->MaybeActivateHighlightedView())
        return;
      break;
    }
    default: {
      // Window activation change happens after overview start animation is
      // finished for performance reasons. During the animation, the focused
      // window prior to entering overview still has focus so stop events from
      // reaching it. See https://crbug.com/951324 for more details.
      if (shell->overview_controller()->IsInStartAnimation())
        break;
      return;
    }
  }

  event->SetHandled();
  event->StopPropagation();
}

void OverviewSession::OnShellDestroying() {
  // Cancel selection will call |Shutdown()|, which will remove observer.
  EndOverview(OverviewEndAction::kShuttingDown);
}

void OverviewSession::OnShelfAlignmentChanged(aura::Window* root_window,
                                              ShelfAlignment old_alignment) {
  // Helper to check if a shelf alignment change results in different
  // visuals for overivew purposes.
  auto same_effective_alignment = [](ShelfAlignment prev,
                                     ShelfAlignment curr) -> bool {
    auto bottom = ShelfAlignment::kBottom;
    auto locked = ShelfAlignment::kBottomLocked;
    return (prev == bottom && curr == locked) ||
           (prev == locked && curr == bottom);
  };

  // On changing from kBottomLocked to kBottom shelf alignment or vice versa
  // (usually from entering/exiting lock screen), keep splitview if it's active.
  // Done here instead of using a SessionObserver so we can skip the
  // EndOverview() at the end of this function if necessary.
  ShelfAlignment current_alignment = Shelf::ForWindow(root_window)->alignment();
  if (SplitViewController::Get(root_window)->InSplitViewMode() &&
      same_effective_alignment(old_alignment, current_alignment)) {
    return;
  }

  // When the shelf alignment changes while in overview, the display work area
  // doesn't get updated anyways (see https://crbug.com/834400). In this case,
  // even updating the grid bounds won't make any difference, so we simply exit
  // overview.
  EndOverview(OverviewEndAction::kShelfAlignmentChanged);
}

void OverviewSession::OnUserWorkAreaInsetsChanged(aura::Window* root_window) {
  // Don't make any change if |root_window| is not the primary root window.
  // Because ChromveVox is only shown on the primary window.
  if (root_window != Shell::GetPrimaryRootWindow())
    return;

  const bool new_chromevox_enabled =
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
  // Don't make any change if ChromeVox status remains the same.
  if (new_chromevox_enabled == chromevox_enabled_)
    return;

  // Make ChromeVox status up to date.
  chromevox_enabled_ = new_chromevox_enabled;

  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_) {
    // Do not handle work area insets change in |overview_grid| if its root
    // window doesn't match |root_window|.
    if (root_window == overview_grid->root_window())
      overview_grid->OnUserWorkAreaInsetsChanged(root_window);
  }
}

void OverviewSession::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  // Do nothing if overview is being shutdown.
  if (!Shell::Get()->overview_controller()->InOverviewSession())
    return;

  RefreshNoWindowsWidgetBoundsOnEachGrid(/*animate=*/false);
}

void OverviewSession::OnSplitViewDividerPositionChanged() {
  RefreshNoWindowsWidgetBoundsOnEachGrid(/*animate=*/false);
}

void OverviewSession::OnTabletModeStarted() {
  OnTabletModeChanged();
}

void OverviewSession::OnTabletModeEnded() {
  OnTabletModeChanged();
}

void OverviewSession::OnTabletModeChanged() {
  DCHECK(saved_desk_util::IsSavedDesksEnabled());
  DCHECK(saved_desk_presenter_);
  saved_desk_presenter_->UpdateUIForSavedDeskLibrary();
}

void OverviewSession::Move(bool reverse) {
  // Do not allow moving the highlight while in the middle of a drag.
  if (window_util::IsAnyWindowDragged() || desks_util::IsDraggingAnyDesk())
    return;

  highlight_controller_->MoveHighlight(reverse);
}

bool OverviewSession::ProcessForScrolling(const ui::KeyEvent& event) {
  if (!ShouldUseTabletModeGridLayout())
    return false;

  // TODO(sammiequon): This only works for tablet mode at the moment, so using
  // the primary display works. If this feature is adapted for multi display
  // then this needs to be revisited.
  auto* grid = GetGridWithRootWindow(Shell::GetPrimaryRootWindow());
  const bool press = (event.type() == ui::ET_KEY_PRESSED);

  if (!press) {
    if (is_keyboard_scrolling_grid_) {
      is_keyboard_scrolling_grid_ = false;
      grid->EndScroll();
      return true;
    }
    return false;
  }

  // Presses only at this point.
  if (event.key_code() != ui::VKEY_LEFT && event.key_code() != ui::VKEY_RIGHT)
    return false;

  if (!event.IsControlDown())
    return false;

  const bool repeat = event.is_repeat();
  const bool reverse = event.key_code() == ui::VKEY_LEFT;
  if (!repeat) {
    is_keyboard_scrolling_grid_ = true;
    grid->StartScroll();
    grid->UpdateScrollOffset(kKeyboardPressScrollingDp * (reverse ? 1 : -1));
    return true;
  }

  grid->UpdateScrollOffset(kKeyboardHoldScrollingDp * (reverse ? 1 : -1));
  return true;
}

void OverviewSession::RemoveAllObservers() {
  display_observer_.reset();
  active_window_before_overview_observation_.Reset();
  active_window_before_overview_ = nullptr;
}

void OverviewSession::UpdateNoWindowsWidgetOnEachGrid() {
  if (is_shutting_down_)
    return;

  for (auto& grid : grid_list_)
    grid->UpdateNoWindowsWidget(IsEmpty());
}

void OverviewSession::RefreshNoWindowsWidgetBoundsOnEachGrid(bool animate) {
  // If there are overview items then the no windows widgets will not be
  // visible so early return.
  if (!IsEmpty())
    return;

  for (auto& grid : grid_list_)
    grid->RefreshNoWindowsWidgetBounds(animate);
}

void OverviewSession::OnItemAdded(aura::Window* window) {
  ++num_items_;
  UpdateNoWindowsWidgetOnEachGrid();

  OverviewGrid* grid = GetGridWithRootWindow(window->GetRootWindow());
  // The drop target window is non-activatable, so no need to transfer focus.
  if (grid && grid->IsDropTargetWindow(window))
    return;

  // Transfer focus from `window` to `overview_focus_widget_` to match the
  // behavior of entering overview mode in the beginning.
  DCHECK(overview_focus_widget_);
  // `overview_focus_widget_` might not visible yet as `OnItemAdded()` might be
  // called before `OnStartingAnimationComplete()` is called, so use `Show()` or
  // `ShowInactive()` instead of `ActivateWindow()` to show the widget.
  // When the saved desk library is on, do not switch focus to avoid unexpected
  // name commit.
  bool saved_desk_grid_should_keep_focus =
      IsShowingSavedDeskLibrary() || WillShowSavedDeskLibrary();
  if (saved_desk_grid_should_keep_focus)
    overview_focus_widget_->ShowInactive();
  else
    overview_focus_widget_->Show();

  UpdateAccessibilityFocus();
}

bool OverviewSession::ShouldKeepOverviewOpenForSavedDeskDialog(
    aura::Window* gained_active,
    aura::Window* lost_active) {
  DCHECK(saved_desk_util::IsSavedDesksEnabled());
  const views::Widget* dialog_widget =
      saved_desk_dialog_controller_->dialog_widget();
  if (!dialog_widget)
    return false;

  auto* dialog_window = dialog_widget->GetNativeWindow();
  return gained_active == dialog_window || lost_active == dialog_window;
}

void OverviewSession::UpdateFrameThrottling() {
  std::vector<aura::Window*> windows_to_throttle;
  if (!grid_list_.empty()) {
    windows_to_throttle.reserve(grid_list_.size() * grid_list_[0]->size() * 2);
    for (auto& grid : grid_list_) {
      for (auto& item : grid->window_list())
        windows_to_throttle.push_back(item->GetWindow());
    }
  }
  Shell::Get()->frame_throttling_controller()->StartThrottling(
      windows_to_throttle);
}
}  // namespace ash

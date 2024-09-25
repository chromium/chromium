// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_bar_controller.h"

#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/desk_button_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/desks/desk_bar_view.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_button/desk_button.h"
#include "ash/wm/desks/desk_button/desk_button_container.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/display/tablet_state.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

bool ShouldProcessLocatedEvent(const ui::LocatedEvent& event) {
  if (event.type() != ui::EventType::kMousePressed &&
      event.type() != ui::EventType::kTouchPressed) {
    return false;
  }

  if (aura::Window* target = static_cast<aura::Window*>(event.target())) {
    if (aura::Window* container = GetContainerForWindow(target)) {
      if (container->GetId() == kShellWindowId_VirtualKeyboardContainer ||
          container->GetId() == kShellWindowId_MenuContainer) {
        return false;
      }
    }
  }

  return true;
}

bool TargetsSettingsBubbleContainer(const ui::LocatedEvent& event) {
  if (aura::Window* target = static_cast<aura::Window*>(event.target())) {
    if (aura::Window* container = GetContainerForWindow(target)) {
      return container->GetId() == kShellWindowId_SettingBubbleContainer;
    }
  }
  return false;
}

}  // namespace

DeskBarController::BarWidgetAndView::BarWidgetAndView(
    DeskBarViewBase* view,
    std::unique_ptr<views::Widget> widget)
    : bar_widget(std::move(widget)), bar_view(view) {}

DeskBarController::BarWidgetAndView::BarWidgetAndView(
    BarWidgetAndView&& other) = default;

DeskBarController::BarWidgetAndView&
DeskBarController::BarWidgetAndView::operator=(BarWidgetAndView&& other) =
    default;

DeskBarController::BarWidgetAndView::~BarWidgetAndView() = default;

const views::View* DeskBarController::BarWidgetAndView::GetNextFocusableView(
    views::View* starting_view,
    bool reverse) const {
  CHECK(bar_widget);
  auto* focus_manager = bar_widget->GetFocusManager();
  CHECK(focus_manager->ContainsView(starting_view));
  return focus_manager->GetNextFocusableView(
      starting_view, /*starting_widget=*/nullptr, reverse, /*dont_loop=*/false);
}

const views::View* DeskBarController::BarWidgetAndView::GetFirstFocusableView()
    const {
  return GetNextFocusableView(bar_view, /*reverse=*/false);
}

const views::View* DeskBarController::BarWidgetAndView::GetLastFocusableView()
    const {
  return GetNextFocusableView(bar_view, /*reverse=*/true);
}

DeskBarController::DeskBarController() {
  Shell::Get()->overview_controller()->AddObserver(this);
  DesksController::Get()->AddObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
  // TODO(b/301274861): DeskBarController should only be doing pre-target
  // handling when the desk bar is visible.
  Shell::Get()->AddPreTargetHandler(this);
  Shell::Get()->AddShellObserver(this);
}

DeskBarController::~DeskBarController() {
  CloseAllDeskBars();
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->RemovePreTargetHandler(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
  DesksController::Get()->RemoveObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
}

void DeskBarController::OnDeskSwitchAnimationLaunching() {
  CloseAllDeskBars();
}

void DeskBarController::OnMouseEvent(ui::MouseEvent* event) {
  OnLocatedEvent(*event);
}

void DeskBarController::OnTouchEvent(ui::TouchEvent* event) {
  OnLocatedEvent(*event);
}

void DeskBarController::OnKeyEvent(ui::KeyEvent* event) {
  const bool is_key_press = event->type() == ui::EventType::kKeyPressed;

  // We return early if we are in an overview session because the overview desk
  // bar has its own predefined key event handling logic. This will handle key
  // events when the desk bar is not visible because the undo desk close toast
  // can be visible without the desk bar being visible. But we still do not want
  // to encroach on the logic that is established for the overview desk bar.
  if (!is_key_press || IsInOverviewSession()) {
    return;
  }

  // TODO(b/301274861): Move toast highlighting logic outside of the desk bar
  // controller.
  // There are two scenarios in which we should handle the close all undo toast
  // without the desk bar being open:
  // 1) the user presses return. This can occur whether the desk bar is showing
  //    or not, and in either case it should activate the undo toast.
  // 2) the user presses any key other than return when the desk bar is not
  //    showing. If this is the case, we should try to close the toast.
  DesksController* desks_controller = DesksController::Get();
  if (event->key_code() == ui::VKEY_RETURN) {
    // Fallthrough to the toast and let its button handle the return key.
    return;
  }

  if (!IsShowingDeskBar()) {
    if (event->key_code() != ui::VKEY_RETURN) {
      desks_controller->MaybeDismissPersistentDeskRemovalToast();
    }

    return;
  }

  // If the user is performing any non-traversal action (i.e. they do anything
  // other than press tab or an arrow key) and is not trying to undo desk
  // removal with Ctrl + Z, we should close the desk removal undo toast.
  const ui::KeyboardCode traversal_keys[7] = {
      ui::VKEY_TAB,   ui::VKEY_UP,    ui::VKEY_DOWN,   ui::VKEY_LEFT,
      ui::VKEY_RIGHT, ui::VKEY_SHIFT, ui::VKEY_CONTROL};
  if (!(event->IsControlDown() && event->key_code() == ui::VKEY_Z) &&
      !base::Contains(traversal_keys, event->key_code())) {
    desks_controller->MaybeDismissPersistentDeskRemovalToast();
  }

  const bool is_control_down = event->IsControlDown();
  for (auto& desk_bar : desk_bars_) {
    if (!desk_bar.bar_view->GetVisible()) {
      continue;
    }

    auto* focus_manager = desk_bar.bar_widget->GetFocusManager();
    views::View* focused_view = focus_manager->GetFocusedView();
    const bool focused_name_view =
        views::IsViewClass<DeskNameView>(focused_view);

    // TODO(b/290651821): Consolidates arrow key behaviors for the desk bar.
    switch (event->key_code()) {
      case ui::VKEY_BROWSER_BACK:
      case ui::VKEY_ESCAPE: {
        if (focused_name_view) {
          return;
        }
        base::AutoReset<bool> auto_reset(&should_desk_button_acquire_focus_,
                                         true);
        CloseAllDeskBars();
        break;
      }
      case ui::VKEY_UP:
      case ui::VKEY_DOWN:
        MoveFocus(desk_bar, /*reverse=*/event->key_code() == ui::VKEY_UP);
        break;
      case ui::VKEY_TAB:
        // For alt+tab/alt+shift+tab, like other UIs on the shelf, it should
        // hide the desk bars then show the window cycle list.
        if (event->IsAltDown()) {
          return;
        }
        MoveFocus(desk_bar, /*reverse=*/event->IsShiftDown());
        break;
      case ui::VKEY_LEFT:
      case ui::VKEY_RIGHT:
        // Let the textfield handle left/right to move the caret, unless using
        // ChromeVox traversal.
        if (!event->IsCommandDown() && focused_name_view) {
          return;
        }
        // Control + left/right falls through to be handed by the desk preview
        // to swap desks.
        if (is_control_down) {
          return;
        }
        MoveFocus(desk_bar, /*reverse=*/event->key_code() == ui::VKEY_LEFT);
        break;
      case ui::VKEY_Z:
        // Ctrl + Z undos a close all operation if the toast has not yet
        // expired. Ctrl + Alt + Z triggers ChromeVox so we don't do anything
        // here to interrupt that.
        if (!is_control_down || (is_control_down && event->IsAltDown())) {
          return;
        }

        desks_controller->MaybeCancelDeskRemoval();
        break;
      default:
        return;
    }
  }

  event->SetHandled();
  event->StopPropagation();
}

void DeskBarController::OnOverviewModeWillStart() {
  CloseAllDeskBars();
}

void DeskBarController::OnShellDestroying() {
  is_shell_destroying_ = true;

  // The desk bar widgets should not outlive shell. Unlike `DeleteSoon`, we get
  // rid of it right away.
  desk_bars_.clear();
}

void DeskBarController::OnDisplayTabletStateChanged(
    display::TabletState state) {
  if (state != display::TabletState::kEnteringTabletMode) {
    return;
  }

  CloseAllDeskBars();
}

void DeskBarController::OnWindowActivated(ActivationReason reason,
                                          aura::Window* gained_active,
                                          aura::Window* lost_active) {
  if (is_shell_destroying_ || should_ignore_activation_change_) {
    return;
  }

  // Closing the bar for "press" type events is handled by
  // `ui::EventHandler`. Activation can change when a user merely moves the
  // cursor outside the bar when `FocusFollowsCursor` is enabled, so losing
  // activation should *not* close the bar.
  if (reason == wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT) {
    return;
  }

  // Destroys the bar when it loses activation, or any other window gains
  // activation.
  for (auto& desk_bar : desk_bars_) {
    CHECK(desk_bar.bar_widget->GetNativeWindow());
    if ((lost_active &&
         desk_bar.bar_widget->GetNativeWindow()->Contains(lost_active)) ||
        (gained_active &&
         !desk_bar.bar_widget->GetNativeWindow()->Contains(gained_active))) {
      CloseAllDeskBars();
    }
  }
}

void DeskBarController::OnDisplayMetricsChanged(const display::Display& display,
                                                uint32_t changed_metrics) {
  if (!IsShowingDeskBar()) {
    return;
  }

  for (auto& desk_bar : desk_bars_) {
    if (!desk_bar.bar_view->GetVisible()) {
      continue;
    }

    const int64_t display_id =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(desk_bar.bar_view->root())
            .id();
    if (display.id() != display_id) {
      continue;
    }

    desk_bar.bar_view->UpdateBarBounds();
  }
}

DeskBarViewBase* DeskBarController::GetDeskBarView(aura::Window* root) {
  auto it = base::ranges::find_if(desk_bars_,
                                  [root](const BarWidgetAndView& desk_bar) {
                                    return desk_bar.bar_view->root() == root;
                                  });
  return it != desk_bars_.end() ? it->bar_view : nullptr;
}

bool DeskBarController::IsShowingDeskBar() const {
  return base::ranges::any_of(desk_bars_, [](const BarWidgetAndView& desk_bar) {
    return desk_bar.bar_view->GetVisible();
  });
}

void DeskBarController::OpenDeskBar(aura::Window* root) {
  CHECK(root && root->IsRootWindow());

  if (!window_occlusion_calculator_ &&
      features::IsDeskBarWindowOcclusionOptimizationEnabled()) {
    window_occlusion_calculator_.emplace();
  }

  auto presentation_time_recorder = CreatePresentationTimeHistogramRecorder(
      root->layer()->GetCompositor(), kDeskBarEnterPresentationHistogram, "",
      ui::PresentationTimeRecorder::BucketParams::CreateWithMaximum(
          kDeskBarEnterExitPresentationMaxLatency));
  presentation_time_recorder->RequestNext();

  // It should not close all bars for the activation change when a new desk bar
  // is opening.
  base::AutoReset<bool> auto_reset(&should_ignore_activation_change_, true);

  desk_button_root_ = root;

  SetDeskButtonActivation(root, /*is_activated=*/true);

  // Calculates bar widget and bar view.
  DeskBarViewBase* bar_view = GetDeskBarView(root);
  if (!bar_view) {
    std::unique_ptr<views::Widget> bar_widget =
        DeskBarViewBase::CreateDeskWidget(root, gfx::Rect(),
                                          DeskBarViewBase::Type::kDeskButton);
    // This pattern is unconventional, but we need to show the empty widget here
    // before setting the contents view to prevent the wrong layer being
    // mirrored in `DeskPreviewView`. See b/287116737#comment6 for more details.
    bar_widget->Show();
    bar_view = bar_widget->SetContentsView(std::make_unique<DeskBarView>(
        root, window_occlusion_calculator_
                  ? window_occlusion_calculator_->AsWeakPtr()
                  : nullptr));
    bar_view->Init(bar_widget->GetNativeWindow());

    // Ownership transfer and bookkeeping.
    desk_bars_.emplace_back(bar_view, std::move(bar_widget));
  } else {
    bar_view->GetWidget()->Show();
  }
}

void DeskBarController::CloseDeskBar(aura::Window* root) {
  CHECK(root && root->IsRootWindow());

  // It should not close all bars for the activation change when an existing
  // desk bar is closing.
  base::AutoReset<bool> auto_reset(&should_ignore_activation_change_, true);

  for (auto it = desk_bars_.begin(); it != desk_bars_.end();) {
    if (it->bar_view->root() == root) {
      CloseDeskBarInternal(*it);
      it = desk_bars_.erase(it);
    } else {
      it++;
    }
  }

  if (desk_bars_.empty()) {
    window_occlusion_calculator_.reset();
  }
}

void DeskBarController::CloseAllDeskBars() {
  // It should not close all bars for the activation change when the existing
  // desk bars are closing.
  base::AutoReset<bool> auto_reset(&should_ignore_activation_change_, true);

  for (auto& desk_bar : desk_bars_) {
    if (desk_bar.bar_widget->IsVisible()) {
      CloseDeskBarInternal(desk_bar);
    }
  }

  desk_bars_.clear();
  window_occlusion_calculator_.reset();
}

void DeskBarController::MoveFocus(const BarWidgetAndView& desk_bar,
                                  bool reverse) {
  DesksController* desks_controller = DesksController::Get();
  auto* focus_manager = desk_bar.bar_widget->GetFocusManager();
  views::View* focused_view = focus_manager->GetFocusedView();

  // Focus the first/last focusable view when tabbing out from the toast.
  if (desks_controller->IsUndoToastFocused()) {
    focus_manager->AdvanceFocus(reverse);
    return;
  }

  // Attempt to focus the toast when tabbing out from the first/last focusable
  // view.
  if (focused_view) {
    const views::View* first_focusable_view = desk_bar.GetFirstFocusableView();
    const views::View* last_focusable_view = desk_bar.GetLastFocusableView();
    const views::View* next_focusable_view =
        desk_bar.GetNextFocusableView(focused_view, reverse);
    if ((next_focusable_view == first_focusable_view && !reverse) ||
        (next_focusable_view == last_focusable_view && reverse)) {
      base::AutoReset<bool> auto_reset(&should_ignore_activation_change_, true);
      if (desks_controller->RequestFocusOnUndoDeskRemovalToast()) {
        return;
      }
    }
  }

  // Focus from and to views within the desk bar.
  if (focused_view) {
    focus_manager->AdvanceFocus(reverse);
  } else {
    focus_manager->SetFocusedView(
        desk_bar.bar_view->FindMiniViewForDesk(desks_controller->active_desk())
            ->desk_preview());
  }
}

void DeskBarController::CloseDeskBarInternal(BarWidgetAndView& desk_bar) {
  auto presentation_time_recorder = CreatePresentationTimeHistogramRecorder(
      desk_bar.bar_view->root()->layer()->GetCompositor(),
      kDeskBarExitPresentationHistogram, "",
      ui::PresentationTimeRecorder::BucketParams::CreateWithMaximum(
          kDeskBarEnterExitPresentationMaxLatency));
  presentation_time_recorder->RequestNext();

  desk_bar.bar_widget->Hide();

  // Deletes asynchronously so it is less likely to result in UAF.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, desk_bar.bar_widget.release());

  // Resets `desk_button_root_` when there is no active desk bar.
  if (!IsShowingDeskBar()) {
    desk_button_root_ = nullptr;
  }

  SetDeskButtonActivation(desk_bar.bar_view->root(),
                          /*is_activated=*/false);
}

void DeskBarController::OnLocatedEvent(ui::LocatedEvent& event) {
  if (ShouldProcessLocatedEvent(event)) {
    OnMaybePressOffBar(event);
  }

  // Maybe dismiss the persistent toast, unless the event might be targeting the
  // toast itself. If that is the case, we'd better let the toast handle the
  // event.
  if ((event.type() == ui::EventType::kMousePressed ||
       event.type() == ui::EventType::kTouchPressed) &&
      !TargetsSettingsBubbleContainer(event)) {
    DesksController::Get()->MaybeDismissPersistentDeskRemovalToast();
  }
}

void DeskBarController::OnMaybePressOffBar(ui::LocatedEvent& event) {
  if (desk_bars_.empty()) {
    return;
  }

  // Does nothing for the press within the bar since it is handled by the bar
  // view. Otherwise, we should either commit the desk name changes or close the
  // bars.
  bool intersect_with_bar_view = false;
  bool intersect_with_desk_button = false;
  bool desk_name_being_modified = false;
  for (auto& desk_bar : desk_bars_) {
    // Converts to screen coordinate.
    gfx::Point screen_location;
    gfx::Rect desk_bar_view_bounds = desk_bar.bar_view->GetBoundsInScreen();
    if (event.target()) {
      screen_location = event.target()->GetScreenLocation(event);
    } else {
      screen_location = event.root_location();
      wm::ConvertPointToScreen(desk_bar.bar_view->root(), &screen_location);
    }

    if (desk_bar_view_bounds.Contains(screen_location)) {
      intersect_with_bar_view = true;
    } else if (desk_bar.bar_view->IsDeskNameBeingModified()) {
      desk_name_being_modified = true;
      DeskNameView::CommitChanges(desk_bar.bar_widget.get());
      event.SetHandled();
      event.StopPropagation();
    }

    if (GetDeskButtonContainer(desk_bar.bar_view->root())
            ->IntersectsWithDeskButtonUi(screen_location)) {
      intersect_with_desk_button = true;
    }
  }

  if (!intersect_with_bar_view && !desk_name_being_modified &&
      !intersect_with_desk_button) {
    CloseAllDeskBars();
  }
}

DeskButtonContainer* DeskBarController::GetDeskButtonContainer(
    aura::Window* root) {
  return Shelf::ForWindow(root)->desk_button_widget()->GetDeskButtonContainer();
}

void DeskBarController::SetDeskButtonActivation(aura::Window* root,
                                                bool is_activated) {
  // Store the desk button focus when opening the desk bar.
  if (desk_button_root_ == root && is_activated) {
    Shelf::ForWindow(root)->desk_button_widget()->StoreDeskButtonFocus();
  }

  GetDeskButtonContainer(root)->desk_button()->SetActivation(is_activated);

  // Restore the desk button focus when closing the desk bar.
  if (should_desk_button_acquire_focus_ && desk_button_root_ == root &&
      !is_activated) {
    Shelf::ForWindow(desk_button_root_)
        ->desk_button_widget()
        ->RestoreDeskButtonFocus();
  }
}

}  // namespace ash

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_bar_controller.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/desk_button_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/desks/desk_bar_view.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_button/desk_button.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/presentation_time_recorder.h"
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
  if (event.type() != ui::ET_MOUSE_PRESSED &&
      event.type() != ui::ET_TOUCH_PRESSED) {
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

// Moves the focus ring to the next traversable view.
void MoveFocus(const DeskBarController::BarWidgetAndView& desk_bar,
               bool reverse) {
  auto* focus_manager = desk_bar.bar_widget->GetFocusManager();

  // When ChromeVox is not enabled, we do not need to advance focus outside of
  // the normal focus order (i.e. to a button on a toast that will undo the desk
  // close operation). Therefore, in this case we can just advance focus
  // normally.
  if (!Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    focus_manager->AdvanceFocus(reverse);
    return;
  }

  views::View* focused_view = focus_manager->GetFocusedView();

  views::View* first_focusable_view = desk_bar.GetFirstFocusableView();
  views::View* last_focusable_view = desk_bar.GetLastFocusableView();

  // When a desk is removed and the undo desk removal toast is shown, we return
  // focus back to the start of the cycling order.
  views::View* next_focusable_view;
  DesksController* desks_controller = DesksController::Get();
  if (focused_view) {
    next_focusable_view = desk_bar.GetNextFocusableView(focused_view, reverse);

    // If we are moving over either end of the list of traversible views and
    // there is an active toast with an undo button for desk removal that can be
    // focused, then we unfocus any traversible views while the dismiss button
    // is focused.
    if (((next_focusable_view == first_focusable_view && !reverse) ||
         (next_focusable_view == last_focusable_view && reverse)) &&
        desks_controller->MaybeToggleA11yHighlightOnUndoDeskRemovalToast()) {
      focus_manager->ClearFocus();
      return;
    }
  } else if (reverse &&
             desks_controller
                 ->MaybeToggleA11yHighlightOnUndoDeskRemovalToast()) {
    focus_manager->ClearFocus();
    return;
  }

  if (desks_controller->IsUndoToastHighlighted()) {
    desks_controller->MaybeToggleA11yHighlightOnUndoDeskRemovalToast();
  }

  focus_manager->AdvanceFocus(reverse);
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

views::View* DeskBarController::BarWidgetAndView::GetNextFocusableView(
    views::View* starting_view,
    bool reverse) const {
  CHECK(bar_widget);
  auto* focus_manager = bar_widget->GetFocusManager();
  CHECK(focus_manager->ContainsView(starting_view));
  return focus_manager->GetNextFocusableView(
      starting_view, /*starting_widget=*/nullptr, reverse, /*dont_loop=*/false);
}

views::View* DeskBarController::BarWidgetAndView::GetFirstFocusableView()
    const {
  return GetNextFocusableView(bar_view, /*reverse=*/false);
}

views::View* DeskBarController::BarWidgetAndView::GetLastFocusableView() const {
  return GetNextFocusableView(bar_view, /*reverse=*/true);
}

DeskBarController::DeskBarController() {
  Shell::Get()->overview_controller()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
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
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
}

void DeskBarController::OnDeskSwitchAnimationLaunching() {
  CloseAllDeskBars();
}

void DeskBarController::OnMouseEvent(ui::MouseEvent* event) {
  if (ShouldProcessLocatedEvent(*event)) {
    OnMaybePressOffBar(*event);
  }

  if (event->type() == ui::ET_MOUSE_PRESSED) {
    DesksController::Get()->MaybeDismissPersistentDeskRemovalToast();
  }
}

void DeskBarController::OnTouchEvent(ui::TouchEvent* event) {
  if (ShouldProcessLocatedEvent(*event)) {
    OnMaybePressOffBar(*event);
  }

  if (event->type() == ui::ET_TOUCH_PRESSED) {
    DesksController::Get()->MaybeDismissPersistentDeskRemovalToast();
  }
}

void DeskBarController::OnKeyEvent(ui::KeyEvent* event) {
  const bool is_key_press = event->type() == ui::ET_KEY_PRESSED;

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
    desks_controller->MaybeActivateDeskRemovalUndoButtonOnHighlightedToast();
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
    DeskPreviewView* focused_preview =
        views::AsViewClass<DeskPreviewView>(focused_view);
    DeskNameView* focused_name_view =
        views::AsViewClass<DeskNameView>(focused_view);

    // TODO(b/290651821): Consolidates arrow key behaviors for the desk bar.
    switch (event->key_code()) {
      case ui::VKEY_BROWSER_BACK:
      case ui::VKEY_ESCAPE:
        if (focused_name_view) {
          return;
        }
        CloseAllDeskBars();
        break;
      case ui::VKEY_UP:
      case ui::VKEY_DOWN:
        MoveFocus(desk_bar,
                  /*reverse=*/event->key_code() == ui::VKEY_UP);
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
        if (focused_name_view) {
          return;
        }
        if (is_control_down) {
          if (focused_preview) {
            focused_preview->Swap(
                /*right=*/event->key_code() == ui::VKEY_RIGHT);
          } else {
            return;
          }
        } else {
          MoveFocus(desk_bar,
                    /*reverse=*/event->key_code() == ui::VKEY_LEFT);
        }
        break;
      case ui::VKEY_W:
        if (!is_control_down) {
          return;
        }

        if (focused_preview) {
          focused_preview->Close(
              /*primary_action=*/!event->IsShiftDown());
        } else {
          return;
        }

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

void DeskBarController::OnTabletModeStarting() {
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

DeskBarViewBase* DeskBarController::GetDeskBarView(aura::Window* root) const {
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

  auto presentation_time_recorder = CreatePresentationTimeHistogramRecorder(
      root->layer()->GetCompositor(), kDeskBarEnterPresentationHistogram, "",
      kDeskBarEnterExitPresentationMaxLatency);
  presentation_time_recorder->RequestNext();

  // It should not close all bars for the activation change when a new desk bar
  // is opening.
  base::AutoReset<bool> auto_reset(&should_ignore_activation_change_, true);

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
    bar_view = bar_widget->SetContentsView(std::make_unique<DeskBarView>(root));
    bar_view->Init();

    // Ownership transfer and bookkeeping.
    desk_bars_.emplace_back(bar_view, std::move(bar_widget));
  } else {
    bar_view->GetWidget()->Show();
  }

  SetDeskButtonActivation(root, /*is_activated=*/true);
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
}

void DeskBarController::CloseDeskBarInternal(BarWidgetAndView& desk_bar) {
  auto presentation_time_recorder = CreatePresentationTimeHistogramRecorder(
      desk_bar.bar_view->root()->layer()->GetCompositor(),
      kDeskBarExitPresentationHistogram, "",
      kDeskBarEnterExitPresentationMaxLatency);
  presentation_time_recorder->RequestNext();

  desk_bar.bar_widget->Hide();

  // Deletes asynchronously so it is less likely to result in UAF.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, desk_bar.bar_widget.release());

  SetDeskButtonActivation(desk_bar.bar_view->root(),
                          /*is_activated=*/false);
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
    gfx::Rect desk_button_bounds =
        GetDeskButton(desk_bar.bar_view->root())->GetBoundsInScreen();
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

    if (desk_button_bounds.Contains(screen_location)) {
      intersect_with_desk_button = true;
    }
  }

  if (!intersect_with_bar_view && !desk_name_being_modified &&
      !intersect_with_desk_button) {
    CloseAllDeskBars();
  }
}

DeskButton* DeskBarController::GetDeskButton(aura::Window* root) {
  return Shelf::ForWindow(root)->desk_button_widget()->GetDeskButton();
}

void DeskBarController::SetDeskButtonActivation(aura::Window* root,
                                                bool is_activated) {
  GetDeskButton(root)->SetActivation(/*is_activated=*/is_activated);
}

}  // namespace ash

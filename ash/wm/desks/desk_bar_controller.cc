// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_bar_controller.h"
#include <memory>

#include "ash/public/cpp/shelf_types.h"
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
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/auto_reset.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
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

DeskBarController::DeskBarController() {
  Shell::Get()->overview_controller()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  DesksController::Get()->AddObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
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
}

void DeskBarController::OnTouchEvent(ui::TouchEvent* event) {
  if (ShouldProcessLocatedEvent(*event)) {
    OnMaybePressOffBar(*event);
  }
}

void DeskBarController::OnKeyEvent(ui::KeyEvent* event) {
  const bool is_key_press = event->type() == ui::ET_KEY_PRESSED;
  if (!is_key_press || !IsShowingDeskBar()) {
    return;
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
        focus_manager->AdvanceFocus(/*reverse=*/event->key_code() ==
                                    ui::VKEY_UP);
        break;
      case ui::VKEY_TAB:
        // For alt+tab/alt+shift+tab, like other UIs on the shelf, it should
        // hide the desk bars then show the window cycle list.
        if (event->IsAltDown()) {
          return;
        }
        focus_manager->AdvanceFocus(/*reverse=*/event->IsShiftDown());
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
          focus_manager->AdvanceFocus(/*reverse=*/event->key_code() ==
                                      ui::VKEY_LEFT);
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

        DesksController::Get()->MaybeCancelDeskRemoval();
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

    desk_bar.bar_widget->SetBounds(
        GetDeskBarWidgetBounds(desk_bar.bar_view->root()));
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
    gfx::Rect bounds = GetDeskBarWidgetBounds(root);
    std::unique_ptr<views::Widget> bar_widget =
        DeskBarViewBase::CreateDeskWidget(root, bounds,
                                          DeskBarViewBase::Type::kDeskButton);
    bar_view = bar_widget->SetContentsView(std::make_unique<DeskBarView>(root));
    bar_view->Init();
    // TODO(b/293658108): remove this once the bento bar bounds and layout are
    // correctly set.
    bar_widget->GetRootView()->SetUseDefaultFillLayout(false);

    // Ownership transfer and bookkeeping.
    desk_bars_.emplace_back(bar_view, std::move(bar_widget));
  }

  SetDeskButtonActivation(root, /*is_activated=*/true);
  bar_view->GetWidget()->Show();
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

gfx::Rect DeskBarController::GetDeskBarWidgetBounds(aura::Window* root) const {
  gfx::Rect work_area =
      WorkAreaInsets::ForWindow(root)->user_work_area_bounds();
  gfx::Size bar_size(work_area.width(),
                     DeskBarViewBase::GetPreferredBarHeight(
                         root, DeskBarViewBase::Type::kDeskButton,
                         DeskBarViewBase::State::kExpanded));

  const Shelf* shelf = Shelf::ForWindow(root);
  gfx::Rect shelf_bounds = shelf->GetShelfBoundsInScreen();
  gfx::Rect desk_button_bounds =
      shelf->desk_button_widget()->GetWindowBoundsInScreen();

  gfx::Point bar_origin;
  switch (shelf->alignment()) {
    case ShelfAlignment::kBottom:
      bar_origin.set_x(shelf_bounds.x() +
                       (work_area.width() - bar_size.width()) / 2);
      bar_origin.set_y(shelf_bounds.y() - kDeskBarShelfAndBarSpacing -
                       bar_size.height());
      break;
    case ShelfAlignment::kLeft:
      bar_size.set_width(bar_size.width() - kDeskBarShelfAndBarSpacing);
      bar_origin.set_x(shelf_bounds.right() + kDeskBarShelfAndBarSpacing);
      bar_origin.set_y(desk_button_bounds.y());
      break;
    case ShelfAlignment::kRight:
      bar_size.set_width(bar_size.width() - kDeskBarShelfAndBarSpacing);
      bar_origin.set_x(shelf_bounds.x() - kDeskBarShelfAndBarSpacing -
                       bar_size.width());
      bar_origin.set_y(desk_button_bounds.y());
      break;
    default:
      NOTREACHED_NORETURN();
  }

  return {bar_origin, bar_size};
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

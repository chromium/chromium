// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm_mode/wm_mode_controller.h"

#include <string_view>

#include "ash/capture_mode/capture_mode_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_finder.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_animations.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/window_dimmer.h"
#include "ash/wm_mode/pie_menu_view.h"
#include "ash/wm_mode/wm_mode_button_tray.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/env.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

constexpr gfx::Size kPieMenuSize{300, 300};

WmModeController* g_instance = nullptr;

// The color used to highlight a selected window on hover or tap.
constexpr SkColor kSelectedWindowHighlightColor =
    SkColorSetA(gfx::kGoogleBlue800, 102);  // 40%

std::unique_ptr<WindowDimmer> CreateDimmerForRoot(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  auto dimmer = std::make_unique<WindowDimmer>(
      root->GetChildById(kShellWindowId_MenuContainer), /*animate=*/false);
  dimmer->SetDimColor(kColorAshShieldAndBase40);
  dimmer->window()->Show();
  return dimmer;
}

gfx::Rect GetPieMenuScreenBounds(const gfx::Point& center_point_in_screen,
                                 aura::Window* current_root) {
  CHECK(current_root);

  gfx::Rect bounds(
      gfx::Point(center_point_in_screen.x() - kPieMenuSize.width() / 2,
                 center_point_in_screen.y() - kPieMenuSize.height() / 2),
      kPieMenuSize);
  bounds.AdjustToFit(current_root->GetBoundsInScreen());
  return bounds;
}

// Returns the bounds of the given `window` in screen coordinates, taking into
// account the transformed bounds of it when it's shown in overview.
gfx::Rect GetWindowTargetBoundsInScreen(aura::Window* window) {
  DCHECK(window);

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession()) {
    auto* overview_session = overview_controller->overview_session();
    if (auto* item = overview_session->GetOverviewItemForWindow(window)) {
      return gfx::ToRoundedRect(item->target_bounds());
    }
  }

  return window->GetBoundsInScreen();
}

// Returns the bounds in the root coordinates of the given `window` which should
// be used to highlight it when it's selected by WM Mode.
gfx::Rect GetWindowHighlightBounds(aura::Window* window) {
  gfx::Rect bounds = GetWindowTargetBoundsInScreen(window);
  wm::ConvertRectFromScreen(window->GetRootWindow(), &bounds);
  return bounds;
}

}  // namespace

WmModeController::WmModeController() {
  DCHECK(!g_instance);
  g_instance = this;
  Shell::Get()->AddShellObserver(this);
}

WmModeController::~WmModeController() {
  Shell::Get()->RemoveShellObserver(this);

  // If WM Mode is active, make sure to terminate it now, since it adds itself
  // as a pre-target handler to `aura::Env`, and there's only one instance
  // shared between all `ash_unittests` tests. Otherwise, old `WmModeController`
  // instances from previous tests will spill over to the next tests.
  if (is_active_)
    Toggle();

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
WmModeController* WmModeController::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void WmModeController::Toggle() {
  is_active_ = !is_active_;

  UpdateTrayButtons();
  UpdateDimmers();

  if (is_active_) {
    aura::Env::GetInstance()->AddPreTargetHandler(
        this, ui::EventTarget::Priority::kSystem);
    CreateLayer();
    MaybeChangeRoot(capture_mode_util::GetPreferredRootWindow());
    BuildPieMenu();
    DesksController::Get()->AddObserver(this);
  } else {
    DesksController::Get()->RemoveObserver(this);
    SetSelectedWindow(nullptr);
    pie_menu_widget_.reset();
    pie_menu_view_ = nullptr;
    ReleaseLayer();
    DCHECK(!layer());
    current_root_ = nullptr;
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
  }
}

void WmModeController::OnRootWindowAdded(aura::Window* root_window) {
  if (is_active_)
    dimmers_[root_window] = CreateDimmerForRoot(root_window);
}

void WmModeController::OnRootWindowWillShutdown(aura::Window* root_window) {
  dimmers_.erase(root_window);

  if (root_window == current_root_)
    MaybeChangeRoot(Shell::GetPrimaryRootWindow());
}

void WmModeController::OnMouseEvent(ui::MouseEvent* event) {
  OnLocatedEvent(event);
}

void WmModeController::OnTouchEvent(ui::TouchEvent* event) {
  OnLocatedEvent(event);
}

std::string_view WmModeController::GetLogContext() const {
  return "WmMode";
}

void WmModeController::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());
  gfx::Canvas* canvas = recorder.canvas();
  canvas->DrawColor(SK_ColorTRANSPARENT);

  if (selected_window_) {
    canvas->FillRect(GetWindowHighlightBounds(selected_window_),
                     kSelectedWindowHighlightColor);
  }
}

void WmModeController::OnWindowDestroying(aura::Window* window) {
  CHECK_EQ(window, selected_window_);
  SetSelectedWindow(nullptr);
}

void WmModeController::OnPieMenuButtonPressed(int button_id) {
  if (button_id >= kDeskButtonIdStart && button_id <= kDeskButtonIdEnd) {
    MoveSelectedWindowToDeskAtIndex(button_id - kDeskButtonIdStart);
  }
}

void WmModeController::OnDeskAdded(const Desk* desk, bool from_undo) {
  MaybeRebuildMoveToDeskSubMenu();
}

void WmModeController::OnDeskRemoved(const Desk* desk) {
  MaybeRebuildMoveToDeskSubMenu();

  // Desk removal can mean two desks have been merged, which may affect the
  // bounds of the selected window. Hence, we need to refresh the bounds of the
  // pie menu, and repaint the highlight.
  MaybeRefreshPieMenu();
  ScheduleRepaint();
}

void WmModeController::OnDeskReordered(int old_index, int new_index) {
  MaybeRebuildMoveToDeskSubMenu();
}

void WmModeController::OnDeskActivationChanged(const Desk* activated,
                                               const Desk* deactivated) {
  CHECK(is_active_);
  // The below toggle will turn off WM Mode in response to a desk change.
  Toggle();
}

void WmModeController::OnDeskNameChanged(const Desk* desk,
                                         const std::u16string& new_name) {
  auto* desks_controller = DesksController::Get();
  if (!pie_menu_view_) {
    return;
  }
  const int index = desks_controller->GetDeskIndex(desk);
  CHECK_GE(index, 0);
  pie_menu_view_->SetButtonLabelText(kDeskButtonIdStart + index, new_name);
}

void WmModeController::UpdateDimmers() {
  if (!is_active_) {
    dimmers_.clear();
    return;
  }

  for (aura::Window* root : Shell::GetAllRootWindows()) {
    dimmers_[root] = CreateDimmerForRoot(root);
  }
}

void WmModeController::UpdateTrayButtons() {
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    if (!root_window_controller->GetRootWindow()->is_destroying()) {
      root_window_controller->GetStatusAreaWidget()
          ->wm_mode_button_tray()
          ->UpdateButtonVisuals(is_active_);
    }
  }
}

void WmModeController::OnLocatedEvent(ui::LocatedEvent* event) {
  auto* target = static_cast<aura::Window*>(event->target());

  // Let events targeting the pie menu (if available) go through.
  if (IsTargetingPieMenu(target)) {
    return;
  }

  gfx::Point screen_location = event->root_location();
  wm::ConvertPointToScreen(target->GetRootWindow(), &screen_location);

  // Let events on the WM Mode tray button go through.
  auto* status_area_widget =
      StatusAreaWidget::ForWindow(target->GetRootWindow());
  if (status_area_widget->wm_mode_button_tray()->GetBoundsInScreen().Contains(
          screen_location)) {
    return;
  }

  event->StopPropagation();
  event->SetHandled();

  const bool is_release = event->type() == ui::EventType::kMouseReleased ||
                          event->type() == ui::EventType::kTouchReleased;
  if (!is_release) {
    return;
  }

  base::AutoReset<std::optional<gfx::Point>> reset_release_location(
      &last_release_event_screen_point_, screen_location);

  MaybeChangeRoot(capture_mode_util::GetPreferredRootWindow(screen_location));

  auto* top_most_window = GetTopMostWindowAtPoint(screen_location);
  SetSelectedWindow(top_most_window);
}

void WmModeController::CreateLayer() {
  DCHECK(is_active_);
  DCHECK(!layer());

  Reset(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->set_delegate(this);
  layer()->SetName("WmModeLayer");
}

void WmModeController::MaybeChangeRoot(aura::Window* new_root) {
  DCHECK(is_active_);
  DCHECK(new_root);
  DCHECK(layer());

  if (new_root == current_root_)
    return;

  current_root_ = new_root;
  auto* parent = new_root->GetChildById(kShellWindowId_MenuContainer);
  parent->layer()->Add(layer());
  layer()->SetBounds(parent->bounds());

  SetSelectedWindow(nullptr);
}

void WmModeController::SetSelectedWindow(aura::Window* window) {
  if (selected_window_ != window) {
    if (selected_window_) {
      selected_window_->RemoveObserver(this);
    }

    selected_window_ = window;

    if (selected_window_) {
      selected_window_->AddObserver(this);
    }

    ScheduleRepaint();
  }

  MaybeRefreshPieMenu();
}

void WmModeController::ScheduleRepaint() {
  CHECK(layer());
  layer()->SchedulePaint(layer()->bounds());
}

void WmModeController::BuildPieMenu() {
  DCHECK(!pie_menu_widget_);
  DCHECK(current_root_);

  pie_menu_widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = current_root_->GetChildById(kShellWindowId_MenuContainer);
  params.bounds = gfx::Rect(kPieMenuSize);
  params.name = "WmModePieMenuWidget";
  pie_menu_widget_->Init(std::move(params));
  pie_menu_view_ = pie_menu_widget_->SetContentsView(
      std::make_unique<PieMenuView>(/*delegate=*/this));

  // TODO(b/252558235): Localize once approved.
  pie_menu_view_->main_menu_container()->AddMenuButton(
      kSnapButtonId, u"Snap window", &kWmModeGestureSnapIcon);
  // TODO(b/296643766): Don't add this option for visible-on-all-desks windows.
  pie_menu_view_->main_menu_container()->AddMenuButton(
      kMoveToDeskButtonId, u"Move to desk", &kWmModeGestureMoveToDeskIcon);
  pie_menu_view_->main_menu_container()->AddMenuButton(
      kResizeButtonId, u"Resize window", &kWmModeGestureResizeIcon);

  MaybeRebuildMoveToDeskSubMenu();
}

void WmModeController::MaybeRebuildMoveToDeskSubMenu() {
  if (!pie_menu_view_) {
    return;
  }

  auto* move_to_desk_sub_menu =
      pie_menu_view_->GetOrAddSubMenuForButton(kMoveToDeskButtonId);
  move_to_desk_sub_menu->RemoveAllButtons();

  const auto& desks = DesksController::Get()->desks();
  const int desks_count = desks.size();
  for (int i = 0; i < desks_count; ++i) {
    const int desk_button_id = kDeskButtonIdStart + i;
    DCHECK_LE(desk_button_id, kDeskButtonIdEnd);
    auto* desk = desks[i].get();
    auto* button = move_to_desk_sub_menu->AddMenuButton(
        desk_button_id, desks[i]->name(), &kWmModeGestureMoveToDeskIcon);

    // The button that corresponds to the active desk is dimmed, since windows
    // in the current desk are already on it.
    button->SetEnabled(!desk->is_active());
  }

  pie_menu_view_->DeprecatedLayoutImmediately();
}

bool WmModeController::IsTargetingPieMenu(aura::Window* event_target) const {
  return pie_menu_widget_ && pie_menu_widget_->IsVisible() &&
         pie_menu_widget_->GetNativeWindow()->Contains(event_target);
}

aura::Window* WmModeController::GetTopMostWindowAtPoint(
    const gfx::Point& screen_location) const {
  // Ignore the pie menu if it's available.
  std::set<aura::Window*> windows_to_ignore;
  if (pie_menu_widget_) {
    windows_to_ignore.insert(pie_menu_widget_->GetNativeWindow());
  }
  auto* top_most_window =
      GetTopmostWindowAtPoint(screen_location, windows_to_ignore);
  // Only consider top-most desk windows (i.e. ignore always-on-top, PIP,
  // and Floated windows) for now.
  if (top_most_window &&
      !desks_util::GetDeskContainerForContext(top_most_window)) {
    top_most_window = nullptr;
  }
  return top_most_window;
}

void WmModeController::MaybeRefreshPieMenu() {
  if (!pie_menu_widget_) {
    return;
  }

  if (!selected_window_) {
    // Before we hide the pie menu, we must return to the main menu, so that the
    // next time we show it for a new selected window, it's already showing the
    // main menu.
    pie_menu_view_->ReturnToMainMenu();
    pie_menu_widget_->Hide();
    return;
  }

  pie_menu_widget_->SetBounds(GetPieMenuScreenBounds(
      last_release_event_screen_point_.value_or(
          GetWindowTargetBoundsInScreen(selected_window_).CenterPoint()),
      current_root_));
  pie_menu_widget_->Show();
}

void WmModeController::MoveSelectedWindowToDeskAtIndex(int index) {
  if (!selected_window_) {
    return;
  }

  auto* desks_controller = DesksController::Get();

  // The sideways move-window-to-desk animation is not allowed when overview is
  // active.
  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview_session = overview_controller->InOverviewSession();
  if (!in_overview_session) {
    const int cur_index = desks_controller->GetActiveDeskIndex();
    CHECK_NE(index, cur_index);
    const bool going_left = (index - cur_index) < 0;
    desks_animations::PerformWindowMoveToDeskAnimation(selected_window_,
                                                       going_left);
  }

  desks_controller->MoveWindowFromActiveDeskTo(
      selected_window_, desks_controller->desks()[index].get(),
      selected_window_->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kSendToDesk);

  SetSelectedWindow(nullptr);

  if (in_overview_session) {
    overview_controller->overview_session()->PositionWindows(/*animate=*/true);
  }
}

}  // namespace ash

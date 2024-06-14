// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_highlight_controller.h"

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/border.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

constexpr SkColor kHighlightColor = gfx::kGoogleBlue600;
constexpr int kHighlightSizeFactor = 128;

std::unique_ptr<views::Widget> CreateHighlightWidget(
    const display::Display& display) {
  const int64_t display_id = display.id();

  DCHECK_NE(display_id, display::kInvalidDisplayId);

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = false;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  aura::Window* root = Shell::GetRootWindowForDisplayId(display_id);

  params.parent = root->GetChildById(kShellWindowId_ScreenAnimationContainer);
  params.bounds = root->GetBoundsInRootWindow();
  params.name = "DisplayIdentificationHighlight";

  std::unique_ptr<views::Widget> highlight_widget =
      std::make_unique<views::Widget>();

  const int highlight_thickness =
      std::max(params.bounds.width(), params.bounds.height()) /
      kHighlightSizeFactor;

  highlight_widget->Init(std::move(params));

  highlight_widget->GetRootView()->SetBorder(
      views::CreateSolidBorder(highlight_thickness, kHighlightColor));

  auto* window = highlight_widget->GetNativeWindow();
  window->SetId(kShellWindowId_DisplayIdentificationHighlightWindow);
  ::wm::SetWindowVisibilityAnimationTransition(window, ::wm::ANIMATE_NONE);

  FullscreenMagnifierController* magnification_controller =
      Shell::Get()->fullscreen_magnifier_controller();

  // Forces a redraw of full-screen magnification in order to reverse
  // magnification on display highlight window performed in
  // FullscreenMagnifierController::ReDraw(). If redraw is not forced, then the
  // highlight may not show up around the edges of the display properly until
  // the next redraw.
  if (magnification_controller->IsEnabled()) {
    magnification_controller->MoveWindow(
        magnification_controller->GetWindowPosition(), false);
  }

  highlight_widget->Show();

  return highlight_widget;
}

}  // namespace

DisplayHighlightController::DisplayHighlightController() {
  Shell* shell = Shell::Get();
  SessionControllerImpl* session_controller = shell->session_controller();

  session_controller->AddObserver(this);
  shell->display_manager()->AddDisplayManagerObserver(this);

  is_locked_ = session_controller->IsScreenLocked();
}

DisplayHighlightController::~DisplayHighlightController() {
  Shell* shell = Shell::Get();

  shell->display_manager()->RemoveDisplayManagerObserver(this);
  shell->session_controller()->RemoveObserver(this);
}

void DisplayHighlightController::UpdateDisplayIdentificationHighlight() {
  if (selected_display_id_ == display::kInvalidDisplayId) {
    highlight_widget_.reset();
    return;
  }

  display::DisplayManager* display_manager = Shell::Get()->display_manager();

  // If |selected_display_id_| does not correspond to an active display, we
  // cannot display highlights.
  if (!display_manager->IsActiveDisplayId(selected_display_id_)) {
    highlight_widget_.reset();
    return;
  }

  // If there is only one display, we don't need to show a special highlight for
  // it since there is only one place for the user to look.
  if (display_manager->GetNumDisplays() == 1) {
    highlight_widget_.reset();
    return;
  }

  // Don't show a highlight if the device is locked to ensure that the highlight
  // does not appear on the login screen.
  if (is_locked_) {
    highlight_widget_.reset();
    return;
  }

  highlight_widget_ = CreateHighlightWidget(
      display_manager->GetDisplayForId(selected_display_id_));
}

void DisplayHighlightController::OnLockStateChanged(bool locked) {
  is_locked_ = locked;
  UpdateDisplayIdentificationHighlight();
}

void DisplayHighlightController::OnDidApplyDisplayChanges() {
  UpdateDisplayIdentificationHighlight();
}

void DisplayHighlightController::OnDisplaysInitialized() {
  UpdateDisplayIdentificationHighlight();
}

void DisplayHighlightController::SetHighlightedDisplay(int64_t display_id) {
  if (selected_display_id_ != display_id) {
    selected_display_id_ = display_id;
    UpdateDisplayIdentificationHighlight();
  }
}

}  // namespace ash

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm_mode/wm_mode_controller.h"

#include "ash/capture_mode/capture_mode_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_finder.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_dimmer.h"
#include "ash/wm_mode/wm_mode_button_tray.h"
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
  } else {
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

base::StringPiece WmModeController::GetLogContext() const {
  return "WmMode";
}

void WmModeController::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());
  gfx::Canvas* canvas = recorder.canvas();
  canvas->DrawColor(SK_ColorTRANSPARENT);

  if (selected_window_)
    canvas->FillRect(selected_window_->bounds(), kSelectedWindowHighlightColor);
}

bool WmModeController::IsRootWindowDimmedForTesting(aura::Window* root) const {
  return dimmers_.contains(root);
}

void WmModeController::UpdateDimmers() {
  if (!is_active_) {
    dimmers_.clear();
    return;
  }

  for (auto* root : Shell::GetAllRootWindows())
    dimmers_[root] = CreateDimmerForRoot(root);
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
  gfx::Point screen_location = event->root_location();
  wm::ConvertPointToScreen(target->GetRootWindow(), &screen_location);

  MaybeChangeRoot(capture_mode_util::GetPreferredRootWindow(screen_location));

  // Let events on the WM Mode tray button go through.
  auto* status_area_widget =
      StatusAreaWidget::ForWindow(target->GetRootWindow());
  if (status_area_widget->wm_mode_button_tray()->GetBoundsInScreen().Contains(
          screen_location)) {
    return;
  }

  event->StopPropagation();
  event->SetHandled();

  // Only consider top-most desk windows (i.e. ignore always-on-top, PIP,
  // and Floated windows) for now.
  auto* top_most_window = GetTopmostWindowAtPoint(screen_location, {});
  if (top_most_window &&
      !desks_util::GetDeskContainerForContext(top_most_window)) {
    top_most_window = nullptr;
  }

  // Only repaint if there's a change in the selected window.
  if (selected_window_ != top_most_window) {
    selected_window_ = top_most_window;
    layer()->SchedulePaint(layer()->bounds());
  }
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
  auto* parent = new_root->GetChildById(kShellWindowId_OverlayContainer);
  parent->layer()->Add(layer());
  layer()->SetBounds(parent->bounds());
}

}  // namespace ash

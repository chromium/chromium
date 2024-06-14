// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_observer_hud.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace ash {

TouchObserverHud::TouchObserverHud(aura::Window* initial_root,
                                   const std::string& widget_name)
    : display_id_(GetRootWindowSettings(initial_root)->display_id),
      root_window_(initial_root),
      widget_(new views::Widget()) {
  const display::Display& display =
      Shell::Get()->display_manager()->GetDisplayForId(display_id_);

  auto content = std::make_unique<views::View>();

  const gfx::Size& display_size = display.size();
  content->SetSize(display_size);

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = false;
  params.bounds = display.bounds();
  params.parent =
      Shell::GetContainer(root_window_, kShellWindowId_OverlayContainer);
  params.name = widget_name;
  widget_->Init(std::move(params));
  widget_->SetContentsView(std::move(content));
  widget_->StackAtTop();
  widget_->Show();

  widget_->AddObserver(this);

  // Observe changes in display size and mode to update touch HUD.
  Shell::Get()->display_configurator()->AddObserver(this);
  Shell::Get()->display_manager()->AddDisplayManagerObserver(this);
  root_window_->AddPreTargetHandler(this);
}

TouchObserverHud::~TouchObserverHud() {
  Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
  Shell::Get()->display_configurator()->RemoveObserver(this);

  widget_->RemoveObserver(this);
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void TouchObserverHud::Remove() {
  root_window_->RemovePreTargetHandler(this);

  RootWindowController* controller =
      RootWindowController::ForWindow(root_window_);
  UnsetHudForRootWindowController(controller);

  widget_->CloseNow();
}

void TouchObserverHud::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, widget_);
  delete this;
}

void TouchObserverHud::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  for (const auto& display : removed_displays) {
    if (display.id() == display_id_) {
      widget_->CloseNow();
      break;
    }
  }
}

void TouchObserverHud::OnDisplayMetricsChanged(const display::Display& display,
                                               uint32_t metrics) {
  if (display.id() != display_id_ || !(metrics & DISPLAY_METRIC_BOUNDS))
    return;

  widget_->SetSize(display.size());
}

void TouchObserverHud::OnDisplayConfigurationChanged(
    const display::DisplayConfigurator::DisplayStateList& outputs) {
  // Clear touch HUD for any change in display state (single, dual extended,
  // dual mirrored, ...).
  Clear();
}

void TouchObserverHud::OnDisplaysInitialized() {
  OnDidApplyDisplayChanges();
}

void TouchObserverHud::OnWillApplyDisplayChanges() {
  if (!root_window_)
    return;

  root_window_->RemovePreTargetHandler(this);

  RootWindowController* controller =
      RootWindowController::ForWindow(root_window_);
  UnsetHudForRootWindowController(controller);

  views::Widget::ReparentNativeView(
      widget_->GetNativeView(),
      Shell::GetContainer(root_window_, kShellWindowId_UnparentedContainer));

  root_window_ = nullptr;
}

void TouchObserverHud::OnDidApplyDisplayChanges() {
  if (root_window_)
    return;

  root_window_ = Shell::GetRootWindowForDisplayId(display_id_);

  views::Widget::ReparentNativeView(
      widget_->GetNativeView(),
      Shell::GetContainer(root_window_, kShellWindowId_OverlayContainer));

  RootWindowController* controller =
      RootWindowController::ForWindow(root_window_);
  SetHudForRootWindowController(controller);

  root_window_->AddPreTargetHandler(this);
}

}  // namespace ash

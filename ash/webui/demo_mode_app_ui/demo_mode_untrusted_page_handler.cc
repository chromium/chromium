// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/demo_mode_app_ui/demo_mode_untrusted_page_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/desks/desks_util.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

// If it is playing attract loop, move the app window to the always on top
// wallpaper container so it won't break by other ui view.
void UpdateDemoAppParentWindow(bool is_in_attract_loop,
                               views::Widget* demo_app_widget) {
  auto* app_window = demo_app_widget->GetNativeWindow();
  auto* root_window = app_window->parent()->GetRootWindow();

  if (is_in_attract_loop) {
    // If it is playing animation, move the app to always on top wallpaper
    // container.
    aura::Window* screen_saver_container =
        root_window->GetChildById(kShellWindowId_AlwaysOnTopWallpaperContainer);
    if (screen_saver_container) {
      screen_saver_container->AddChild(app_window);
    }

  } else {
    // If it is not playing animation, move the app back to active desk
    // container so that user can interact with elements out side of the app
    // window.
    desks_util::GetActiveDeskContainerForRoot(root_window)
        ->AddChild(app_window);
  }
}
}  // namespace

DemoModeUntrustedPageHandler::DemoModeUntrustedPageHandler(
    mojo::PendingReceiver<mojom::demo_mode::UntrustedPageHandler>
        pending_receiver,
    views::Widget* widget,
    DemoModeAppDelegate* demo_mode_app_delegate)
    : receiver_(this, std::move(pending_receiver)),
      widget_(widget),
      demo_mode_app_delegate_(demo_mode_app_delegate) {}

DemoModeUntrustedPageHandler::~DemoModeUntrustedPageHandler() = default;

void DemoModeUntrustedPageHandler::ToggleFullscreen() {
  bool is_new_state_fullscreen = !widget_->IsFullscreen();
  widget_->SetFullscreen(is_new_state_fullscreen);
  demo_mode_app_delegate_->RemoveSplashScreen();

  if (features::IsDemoModeAppResetWindowContainerEnable()) {
    // `DemoModeUntrustedPageHandler::ToggleFullscreen` is only invoked by Demo
    // app when it toggles attract loop animation. We could assume that "is
    // fullscreen" equals to "is playing attract loop animation".
    UpdateDemoAppParentWindow(is_new_state_fullscreen, widget_);
  }
}

void DemoModeUntrustedPageHandler::LaunchApp(const std::string& app_id) {
  demo_mode_app_delegate_->LaunchApp(app_id);
}

}  // namespace ash

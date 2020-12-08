// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/immersive_context_lacros.h"

#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/views/widget/widget.h"

ImmersiveContextLacros::ImmersiveContextLacros() = default;

ImmersiveContextLacros::~ImmersiveContextLacros() = default;

void ImmersiveContextLacros::OnEnteringOrExitingImmersive(
    chromeos::ImmersiveFullscreenController* controller,
    bool entering) {
  aura::Window* window = controller->widget()->GetNativeWindow();
  // Lacros is based on Ozone/Wayland, which uses ui::PlatformWindow and
  // aura::WindowTreeHostPlatform.
  auto* wth_platform =
      static_cast<aura::WindowTreeHostPlatform*>(window->GetHost());
  ui::PlatformWindow* platform_window = wth_platform->platform_window();

  auto* wayland_extension = ui::GetWaylandExtension(*platform_window);
  DCHECK(wayland_extension)
      << "Exo Wayland extensions are always present in Lacros.";
  wayland_extension->SetImmersiveFullscreenStatus(entering);
}

gfx::Rect ImmersiveContextLacros::GetDisplayBoundsInScreen(
    views::Widget* widget) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());
  return display.bounds();
}

bool ImmersiveContextLacros::DoesAnyWindowHaveCapture() {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

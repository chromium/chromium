// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/snap_controller_lacros.h"

#include "base/notreached.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/platform_window/extensions/wayland_extension.h"

namespace {

ui::WaylandWindowSnapDirection ToWaylandWindowSnapDirection(
    chromeos::SnapDirection snap) {
  switch (snap) {
    case chromeos::SnapDirection::kNone:
      return ui::WaylandWindowSnapDirection::kNone;
    case chromeos::SnapDirection::kLeft:
      return ui::WaylandWindowSnapDirection::kLeft;
    case chromeos::SnapDirection::kRight:
      return ui::WaylandWindowSnapDirection::kRight;
  }
}

ui::WaylandExtension* WaylandExtensionForAuraWindow(aura::Window* window) {
  // Lacros is based on Ozone/Wayland, which uses ui::PlatformWindow and
  // aura::WindowTreeHostPlatform.
  auto* wth_platform =
      static_cast<aura::WindowTreeHostPlatform*>(window->GetHost());
  ui::PlatformWindow* platform_window = wth_platform->platform_window();

  auto* wayland_extension = ui::GetWaylandExtension(*platform_window);
  DCHECK(wayland_extension)
      << "Exo Wayland extensions are always present in Lacros.";
  return wayland_extension;
}

}  // namespace

SnapControllerLacros::SnapControllerLacros() = default;
SnapControllerLacros::~SnapControllerLacros() = default;

bool SnapControllerLacros::CanSnap(aura::Window* window) {
  // TODO(https://crbug.com/1141701): Implement this method similarly to
  // ash::WindowState::CanSnap().
  return true;
}
void SnapControllerLacros::ShowSnapPreview(aura::Window* window,
                                           chromeos::SnapDirection snap) {
  auto* wayland_extension = WaylandExtensionForAuraWindow(window);
  wayland_extension->ShowSnapPreview(ToWaylandWindowSnapDirection(snap));
}

void SnapControllerLacros::CommitSnap(aura::Window* window,
                                      chromeos::SnapDirection snap) {
  auto* wayland_extension = WaylandExtensionForAuraWindow(window);
  wayland_extension->CommitSnap(ToWaylandWindowSnapDirection(snap));
}

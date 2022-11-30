// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/host/ash_window_tree_host_mirroring_unified.h"
#include "ash/host/ash_window_tree_host_platform.h"
#include "ash/host/ash_window_tree_host_unified.h"
#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ash {
namespace {

bool GetAllowConfineCursor() {
  return base::SysInfo::IsRunningOnChromeOS() ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAshConstrainPointerToRoot);
}

}  // namespace

AshWindowTreeHost::AshWindowTreeHost()
    : allow_confine_cursor_(GetAllowConfineCursor()) {}

AshWindowTreeHost::~AshWindowTreeHost() = default;

void AshWindowTreeHost::TranslateLocatedEvent(ui::LocatedEvent* event) {
  if (event->IsTouchEvent())
    return;

  aura::WindowTreeHost* wth = AsWindowTreeHost();
  aura::Window* root_window = wth->window();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  gfx::Rect local(wth->GetBoundsInPixels().size());
  local.Inset(GetHostInsets());

  if (screen_position_client && !local.Contains(event->location())) {
    gfx::Point location(event->location());
    // In order to get the correct point in screen coordinates
    // during passive grab, we first need to find on which host window
    // the mouse is on, and find out the screen coordinates on that
    // host window, then convert it back to this host window's coordinate.
    screen_position_client->ConvertHostPointToScreen(root_window, &location);
    screen_position_client->ConvertPointFromScreen(root_window, &location);
    wth->ConvertDIPToPixels(&location);

    event->set_location(location);
    event->set_root_location(location);
  }
}

// static
std::unique_ptr<AshWindowTreeHost> AshWindowTreeHost::Create(
    const AshWindowTreeHostInitParams& init_params) {
  if (init_params.mirroring_unified) {
    return std::make_unique<AshWindowTreeHostMirroringUnified>(
        init_params.initial_bounds, init_params.display_id,
        init_params.delegate);
  }
  if (init_params.offscreen) {
    return std::make_unique<AshWindowTreeHostUnified>(
        init_params.initial_bounds, init_params.delegate,
        init_params.compositor_memory_limit_mb);
  }
  ui::PlatformWindowInitProperties properties{init_params.initial_bounds};
  properties.enable_compositing_based_throttling = true;
  properties.compositor_memory_limit_mb =
      init_params.compositor_memory_limit_mb;
  return std::make_unique<AshWindowTreeHostPlatform>(std::move(properties),
                                                     init_params.delegate);
}

}  // namespace ash

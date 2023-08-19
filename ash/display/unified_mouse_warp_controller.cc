// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/unified_mouse_warp_controller.h"

#include <cmath>

#include "ash/display/display_util.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/shell.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/util/display_manager_util.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

AshWindowTreeHost* GetMirroringAshWindowTreeHostForDisplayId(
    int64_t display_id) {
  return Shell::Get()
      ->window_tree_host_manager()
      ->mirror_window_controller()
      ->GetAshWindowTreeHostForDisplayId(display_id);
}

const aura::WindowTreeHost* GetMirroringSourceHostForCurrentEvent() {
  return Shell::Get()
      ->window_tree_host_manager()
      ->mirror_window_controller()
      ->current_event_targeter_src_host();
}

}  // namespace

UnifiedMouseWarpController::UnifiedMouseWarpController()
    : update_location_for_test_(false), display_boundaries_computed_(false) {}

UnifiedMouseWarpController::~UnifiedMouseWarpController() = default;

bool UnifiedMouseWarpController::WarpMouseCursor(ui::MouseEvent* event) {
  // Mirroring windows are created asynchronously, so compute the edge
  // beounds when we received an event instead of in constructor.
  if (!display_boundaries_computed_)
    ComputeBounds();

  aura::Window* target = static_cast<aura::Window*>(event->target());
  gfx::Point point_in_unified_host = event->location();
  ::wm::ConvertPointToScreen(target, &point_in_unified_host);
  // The display bounds of the mirroring windows isn't scaled, so
  // transform back to the host coordinates.
  point_in_unified_host =
      target->GetHost()->GetRootTransform().MapPoint(point_in_unified_host);

  // A native event may not exist in unit test.
  if (!event->HasNativeEvent())
    return false;

  // TODO(dnicoara): crbug.com/415680 Move cursor warping into Ozone once Ozone
  // has access to the logical display layout.
  // Native events in Ozone are in the native window coordinate system. We need
  // to translate them to get the global position.
  const auto* host = GetMirroringSourceHostForCurrentEvent();
  if (!host)
    return false;

  gfx::Point point_in_native =
      ui::EventSystemLocationFromNative(event->native_event());
  point_in_native.Offset(host->GetBoundsInPixels().x(),
                         host->GetBoundsInPixels().y());

  // TODO(afakhry): Remove implicit grab. crbug.com/773348.
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          const_cast<aura::Window*>(host->window()));
  return WarpMouseCursorInNativeCoords(display.id(), point_in_native,
                                       point_in_unified_host,
                                       update_location_for_test_);
}

void UnifiedMouseWarpController::SetEnabled(bool enabled) {
  // Mouse warp should be always on in Unified mode.
}

void UnifiedMouseWarpController::ComputeBounds() {
  display::Displays display_list =
      Shell::Get()->display_manager()->software_mirroring_display_list();

  if (display_list.size() < 2) {
    LOG(ERROR) << "Mirroring Display lost during re-configuration";
    return;
  }

  for (size_t i = 0; i < display_list.size() - 1; ++i) {
    const display::Display& first = display_list[i];
    for (size_t j = i + 1; j < display_list.size(); ++j) {
      const display::Display& second = display_list[j];
      gfx::Rect first_edge;
      gfx::Rect second_edge;
      if (display::ComputeBoundary(first, second, &first_edge, &second_edge)) {
        first_edge = GetNativeEdgeBounds(
            GetMirroringAshWindowTreeHostForDisplayId(first.id()), first_edge);
        second_edge = GetNativeEdgeBounds(
            GetMirroringAshWindowTreeHostForDisplayId(second.id()),
            second_edge);

        displays_edges_map_[first.id()].emplace_back(first.id(), second.id(),
                                                     first_edge);
        displays_edges_map_[second.id()].emplace_back(second.id(), first.id(),
                                                      second_edge);
      }
    }
  }

  display_boundaries_computed_ = true;
}

bool UnifiedMouseWarpController::WarpMouseCursorInNativeCoords(
    int64_t source_display,
    const gfx::Point& point_in_native,
    const gfx::Point& point_in_unified_host,
    bool update_mouse_location_now) {
  const auto edges_iter = displays_edges_map_.find(source_display);
  if (edges_iter == displays_edges_map_.end())
    return false;

  const std::vector<DisplayEdge>& potential_edges = edges_iter->second;
  for (const auto& edge : potential_edges) {
    if (edge.edge_native_bounds_in_source_display.Contains(point_in_native)) {
      AshWindowTreeHost* target_ash_host =
          GetMirroringAshWindowTreeHostForDisplayId(edge.target_display_id);
      MoveCursorTo(target_ash_host, point_in_unified_host,
                   update_mouse_location_now);
      return true;
    }
  }

  return false;
}

}  // namespace ash

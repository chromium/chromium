// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/arc_window_handler.h"

#include "ash/public/cpp/ash_features.h"
#include "components/arc/arc_util.h"
#include "components/exo/shell_surface_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"

namespace {

base::Optional<double> GetDisplayScaleFactor(int64_t display_id) {
  display::Display display;
  if (display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id,
                                                            &display)) {
    return display.device_scale_factor();
  }
  return base::nullopt;
}

void ScaleToRoundedRect(apps::mojom::Rect* rect, double scale_factor) {
  if (rect == nullptr)
    return;
  auto res_rect = gfx::ScaleToRoundedRect(
      gfx::Rect(rect->x, rect->y, rect->width, rect->height), scale_factor);
  rect->x = res_rect.x();
  rect->y = res_rect.y();
  rect->width = res_rect.width();
  rect->height = res_rect.height();
}

}  // namespace

namespace chromeos {
namespace full_restore {

bool IsArcGhostWindowEnabled() {
  return ash::features::IsFullRestoreEnabled() && arc::IsArcVmEnabled();
}

apps::mojom::WindowInfoPtr HandleArcWindowInfo(
    apps::mojom::WindowInfoPtr window_info) {
  // Remove ARC bounds info if the ghost window disabled. The bounds will
  // be controlled by ARC.
  if (!IsArcGhostWindowEnabled()) {
    window_info->bounds.reset();
    return window_info;
  }
  auto scale_factor = GetDisplayScaleFactor(window_info->display_id);
  // Remove ARC bounds info if the the display doesn't exist. The bounds will
  // be controlled by ARC.
  if (!scale_factor.has_value()) {
    window_info->bounds.reset();
    return window_info;
  }
  ScaleToRoundedRect(window_info->bounds.get(), scale_factor.value());
  return window_info;
}

ArcWindowHandler::WindowSessionResolver::WindowSessionResolver(
    ShellSurfaceMap* session_id_map)
    : session_id_map_(session_id_map) {}

void ArcWindowHandler::WindowSessionResolver::PopulateProperties(
    const Params& params,
    ui::PropertyHandler& out_properties_container) {
  if (params.window_session_id <= 0)
    return;
  auto it = session_id_map_->find(params.window_session_id);
  if (it != session_id_map_->end()) {
    if (it->second->HasOverlay())
      it->second->RemoveOverlay();
    SetShellClientControlledShellSurface(&out_properties_container,
                                         it->second.release());
    session_id_map_->erase(it);
  }
}

ArcWindowHandler::ArcWindowHandler() {
  exo::WMHelper::GetInstance()->RegisterAppPropertyResolver(
      std::make_unique<WindowSessionResolver>(&session_id_to_shell_surface_));
}

ArcWindowHandler::~ArcWindowHandler() = default;

void ArcWindowHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}
void ArcWindowHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}
bool ArcWindowHandler::HasObserver(Observer* observer) {
  return observer_list_.HasObserver(observer);
}

void ArcWindowHandler::OnAppInstanceConnected() {
  // TODO(sstan): Send existed ghost window info to ARC once ghost window
  // has been introduced.
  for (auto& observer : observer_list_)
    observer.OnAppInstanceConnected();
}

}  // namespace full_restore
}  // namespace chromeos

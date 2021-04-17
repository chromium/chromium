// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/arc_window_handler.h"

#include "ash/public/cpp/ash_features.h"
#include "components/arc/arc_util.h"
#include "components/exo/shell_surface_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromeos {
namespace full_restore {

bool IsArcGhostWindowEnabled() {
  return ash::features::IsFullRestoreEnabled() && arc::IsArcVmEnabled();
}

apps::mojom::WindowInfoPtr ConvertToArcBounds(
    int64_t display_id,
    apps::mojom::WindowInfoPtr window_info) {
  if (!IsArcGhostWindowEnabled()) {
    window_info->bounds.reset();
    return window_info;
  }
  double scale_factor = 0;
  display::Display display;
  if (display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id,
                                                            &display)) {
    scale_factor = display.device_scale_factor();
  }
  if (scale_factor == 0) {
    window_info->bounds.reset();
    return window_info;
  }
  // TODO(sstan): Only for single screen. Need to find a general way.
  window_info->bounds->x *= scale_factor;
  window_info->bounds->y *= scale_factor;
  window_info->bounds->width *= scale_factor;
  window_info->bounds->height *= scale_factor;
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

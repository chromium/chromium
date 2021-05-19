// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/arc_window_handler.h"

#include "chrome/browser/chromeos/full_restore/arc_ghost_window_shell_surface.h"
#include "chrome/browser/chromeos/full_restore/arc_window_utils.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/wm_helper.h"
#include "components/full_restore/app_restore_data.h"

namespace chromeos {
namespace full_restore {

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

void ArcWindowHandler::LaunchArcGhostWindow(
    const std::string& app_id,
    int32_t session_id,
    ::full_restore::AppRestoreData* restore_data) {
  DCHECK(restore_data);
  DCHECK(restore_data->current_bounds.has_value());
  DCHECK(restore_data->display_id.has_value());

  session_id_to_shell_surface_.emplace(
      session_id,
      InitArcGhostWindow(
          this, app_id, session_id, restore_data->display_id.value(),
          restore_data->current_bounds.value(), restore_data->maximum_size,
          restore_data->minimum_size, restore_data->status_bar_color,
          base::BindRepeating(&ArcWindowHandler::CloseWindow,
                              weak_ptr_factory_.GetWeakPtr(), session_id)));
}

void ArcWindowHandler::CloseWindow(int session_id) {
  auto it = session_id_to_shell_surface_.find(session_id);
  if (it == session_id_to_shell_surface_.end())
    return;

  for (auto& observer : observer_list_)
    observer.OnWindowCloseRequested(session_id);

  session_id_to_shell_surface_.erase(it);
}

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
  is_app_instance_connected_ = true;

  // Send all pending window info updates to ARC.
  for (auto& window_info_pr : session_id_to_pending_window_info_) {
    arc::UpdateWindowInfo(std::move(window_info_pr.second));
  }
  session_id_to_pending_window_info_.clear();

  for (auto& observer : observer_list_)
    observer.OnAppInstanceConnected();
}

void ArcWindowHandler::OnWindowInfoUpdated(int window_id,
                                           int state,
                                           int64_t display_id,
                                           gfx::Rect bounds) {
  auto window_info = arc::mojom::WindowInfo::New();
  window_info->window_id = window_id;
  window_info->display_id = display_id;
  window_info->bounds = gfx::Rect(bounds);
  window_info->state = state;

  if (is_app_instance_connected_) {
    arc::UpdateWindowInfo(std::move(window_info));
    return;
  }

  session_id_to_pending_window_info_[window_info->window_id] =
      std::move(window_info);
}

}  // namespace full_restore
}  // namespace chromeos

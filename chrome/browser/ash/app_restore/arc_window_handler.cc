// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_window_handler.h"

#include "chrome/browser/ash/app_restore/arc_ghost_window_shell_surface.h"
#include "chrome/browser/ash/app_restore/arc_window_utils.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/wm_helper.h"
#include "ui/views/window/caption_button_layout_constants.h"

namespace ash {
namespace full_restore {

ArcWindowHandler::WindowSessionResolver::WindowSessionResolver(
    ArcWindowHandler* handler)
    : handler_(handler) {}

void ArcWindowHandler::WindowSessionResolver::PopulateProperties(
    const Params& params,
    ui::PropertyHandler& out_properties_container) {
  if (params.window_session_id <= 0)
    return;
  auto it =
      handler_->session_id_to_shell_surface_.find(params.window_session_id);
  if (it != handler_->session_id_to_shell_surface_.end()) {
    // Reuse the ghost window instance for real ARC app window.
    if (it->second->HasOverlay())
      it->second->RemoveOverlay();
    views::Widget* widget = it->second->GetWidget();
    if (widget && widget->GetNativeWindow()) {
      widget->GetNativeWindow()->SetProperty(app_restore::kRealArcTaskWindow,
                                             true);
    }
    SetShellClientControlledShellSurface(&out_properties_container,
                                         it->second.release());
    handler_->session_id_to_shell_surface_.erase(it);
    handler_->ghost_window_pop_count_++;
  } else {
    // ARC ghost window instance.
    out_properties_container.SetProperty(app_restore::kRealArcTaskWindow,
                                         false);
  }
}

ArcWindowHandler::ArcWindowHandler() {
  exo::WMHelper::GetInstance()->RegisterAppPropertyResolver(
      std::make_unique<WindowSessionResolver>(this));
  auto* lifetime_manager = exo::WMHelper::GetInstance()->GetLifetimeManager();
  if (lifetime_manager)
    lifetime_manager->AddObserver(this);
}

ArcWindowHandler::~ArcWindowHandler() {
  if (exo::WMHelper::HasInstance()) {
    auto* lifetime_manager = exo::WMHelper::GetInstance()->GetLifetimeManager();
    if (lifetime_manager)
      lifetime_manager->RemoveObserver(this);
  }
}

void ArcWindowHandler::OnDestroyed() {
  // Destroy all ARC ghost window when Wayland server shutdown.
  std::vector<int> session_ids;
  for (const auto& session_id : session_id_to_shell_surface_)
    session_ids.push_back(session_id.first);

  for (auto session_id : session_ids)
    CloseWindow(session_id);

  session_id_to_pending_window_info_.clear();

  auto* lifetime_manager = exo::WMHelper::GetInstance()->GetLifetimeManager();
  lifetime_manager->RemoveObserver(this);
}

bool ArcWindowHandler::LaunchArcGhostWindow(
    const std::string& app_id,
    int32_t session_id,
    app_restore::AppRestoreData* restore_data) {
  DCHECK(restore_data);
  DCHECK(restore_data->current_bounds.has_value());
  DCHECK(restore_data->display_id.has_value());

  gfx::Rect adjust_bounds = restore_data->current_bounds.value_or(gfx::Rect());

  // Replace the screen bounds by root bounds if there is.
  if (restore_data->bounds_in_root.has_value())
    adjust_bounds = restore_data->bounds_in_root.value();
  if (restore_data->window_state_type.has_value() &&
      (restore_data->window_state_type.value() ==
           chromeos::WindowStateType::kDefault ||
       restore_data->window_state_type.value() ==
           chromeos::WindowStateType::kNormal)) {
    adjust_bounds.Inset(gfx::Insets().set_top(
        views::GetCaptionButtonLayoutSize(
            views::CaptionButtonLayoutSize::kNonBrowserCaption)
            .height()));
  }

  auto shell_surface = ArcGhostWindowShellSurface::Create(
      this, app_id, session_id, adjust_bounds, restore_data,
      base::BindRepeating(&ArcWindowHandler::CloseWindow,
                          weak_ptr_factory_.GetWeakPtr(), session_id));
  if (!shell_surface)
    return false;

  session_id_to_shell_surface_.emplace(session_id, std::move(shell_surface));
  return true;
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
}  // namespace ash

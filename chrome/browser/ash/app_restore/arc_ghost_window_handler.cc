// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"

#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_shell_surface.h"
#include "chrome/browser/ash/app_restore/arc_window_utils.h"
#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/wm_helper.h"
#include "ui/views/window/caption_button_layout_constants.h"

namespace ash::full_restore {

namespace {

ArcGhostWindowHandler* g_instance = nullptr;

}  // namespace

void ArcGhostWindowHandler::WindowSessionResolver::PopulateProperties(
    const Params& params,
    ui::PropertyHandler& out_properties_container) {
  if (params.window_session_id <= 0)
    return;
  auto* handler = ArcGhostWindowHandler::Get();
  if (!handler) {
    // TODO(b/291693166): Remove this null check after change the lifecycle of
    // the handler.
    LOG(ERROR) << "ArcGhostWindowHandler haven't been initialized.";
    return;
  }
  auto it =
      handler->session_id_to_shell_surface_.find(params.window_session_id);
  if (it != handler->session_id_to_shell_surface_.end()) {
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
    handler->session_id_to_shell_surface_.erase(it);
    handler->ghost_window_pop_count_++;
  } else {
    // ARC ghost window instance.
    out_properties_container.SetProperty(app_restore::kRealArcTaskWindow,
                                         false);
  }
}

ArcGhostWindowHandler::ArcGhostWindowHandler() {
  DCHECK_EQ(nullptr, g_instance);
  exo::WMHelper::GetInstance()->RegisterAppPropertyResolver(
      std::make_unique<WindowSessionResolver>());
  auto* lifetime_manager = exo::WMHelper::GetInstance()->GetLifetimeManager();
  if (lifetime_manager)
    lifetime_manager->AddObserver(this);
  g_instance = this;
}

ArcGhostWindowHandler::~ArcGhostWindowHandler() {
  DCHECK_EQ(this, g_instance);
  if (exo::WMHelper::HasInstance()) {
    auto* lifetime_manager = exo::WMHelper::GetInstance()->GetLifetimeManager();
    if (lifetime_manager)
      lifetime_manager->RemoveObserver(this);
  }
  for (auto& observer : observer_list_) {
    observer.OnGhostWindowHandlerDestroy();
  }
  g_instance = nullptr;
}

// static
ArcGhostWindowHandler* ArcGhostWindowHandler::Get() {
  return g_instance;
}

void ArcGhostWindowHandler::OnDestroyed() {
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

bool ArcGhostWindowHandler::LaunchArcGhostWindow(
    const std::string& app_id,
    int32_t session_id,
    app_restore::AppRestoreData* restore_data) {
  DCHECK(restore_data);
  DCHECK(restore_data->display_id.has_value());

  const app_restore::WindowInfo& window_info = restore_data->window_info;
  CHECK(window_info.current_bounds.has_value());

  gfx::Rect adjust_bounds = window_info.current_bounds.value_or(gfx::Rect());

  // Replace the screen bounds by root bounds if there is.
  if (window_info.arc_extra_info &&
      window_info.arc_extra_info->bounds_in_root) {
    adjust_bounds = *window_info.arc_extra_info->bounds_in_root;
  }

  if (window_info.window_state_type &&
      chromeos::IsNormalWindowStateType(*window_info.window_state_type)) {
    adjust_bounds.Inset(gfx::Insets().set_top(
        views::GetCaptionButtonLayoutSize(
            views::CaptionButtonLayoutSize::kNonBrowserCaption)
            .height()));
  }

  auto shell_surface = ArcGhostWindowShellSurface::Create(
      app_id, ::arc::GhostWindowType::kFullRestore, session_id, adjust_bounds,
      restore_data,
      base::BindRepeating(&ArcGhostWindowHandler::CloseWindow,
                          weak_ptr_factory_.GetWeakPtr(), session_id));
  if (!shell_surface)
    return false;

  session_id_to_shell_surface_.emplace(session_id, std::move(shell_surface));
  return true;
}

bool ArcGhostWindowHandler::UpdateArcGhostWindowType(
    int32_t session_id,
    arc::GhostWindowType window_type) {
  auto it = session_id_to_shell_surface_.find(session_id);
  if (it == session_id_to_shell_surface_.end())
    return false;
  auto* shell_surface =
      static_cast<ArcGhostWindowShellSurface*>(it->second.get());
  shell_surface->SetWindowType(window_type);
  return true;
}

void ArcGhostWindowHandler::CloseWindow(int session_id) {
  auto it = session_id_to_shell_surface_.find(session_id);
  if (it == session_id_to_shell_surface_.end())
    return;

  for (auto& observer : observer_list_)
    observer.OnWindowCloseRequested(session_id);
  session_id_to_shell_surface_.erase(it);
}

void ArcGhostWindowHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}
void ArcGhostWindowHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}
bool ArcGhostWindowHandler::HasObserver(Observer* observer) {
  return observer_list_.HasObserver(observer);
}

void ArcGhostWindowHandler::OnAppInstanceConnected() {
  is_app_instance_connected_ = true;

  // Send all pending window info updates to ARC.
  for (auto& window_info_pr : session_id_to_pending_window_info_) {
    ::arc::UpdateWindowInfo(std::move(window_info_pr.second));
  }
  session_id_to_pending_window_info_.clear();

  for (auto& observer : observer_list_)
    observer.OnAppInstanceConnected();
}

void ArcGhostWindowHandler::OnAppStatesUpdate(std::string app_id,
                                              bool ready,
                                              bool need_fixup) {
  for (auto& observer : observer_list_)
    observer.OnAppStatesUpdate(app_id, ready, need_fixup);
}

void ArcGhostWindowHandler::OnWindowInfoUpdated(int window_id,
                                                int state,
                                                int64_t display_id,
                                                gfx::Rect bounds) {
  auto window_info = ::arc::mojom::WindowInfo::New();
  window_info->window_id = window_id;
  window_info->display_id = display_id;
  window_info->state = state;
  // Do not override bounds in window info if the window state type is not
  // specified when ghost window launched.
  if (window_info->state !=
      static_cast<int32_t>(chromeos::WindowStateType::kDefault)) {
    window_info->bounds = gfx::Rect(bounds);
  }

  session_id_to_pending_window_info_[window_info->window_id] =
      window_info->Clone();

  if (is_app_instance_connected_) {
    ::arc::UpdateWindowInfo(std::move(window_info));
  }
}

}  // namespace ash::full_restore

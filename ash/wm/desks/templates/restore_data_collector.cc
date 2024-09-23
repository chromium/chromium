// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/restore_data_collector.h"

#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/desk_profiles_delegate.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/templates/saved_desk_dialog_controller.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/uuid.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_info.h"
#include "ui/wm/core/window_util.h"

namespace ash {
RestoreDataCollector::Call::Call()
    : data(std::make_unique<app_restore::RestoreData>()) {}
RestoreDataCollector::Call::Call(RestoreDataCollector::Call&&) = default;
RestoreDataCollector::Call& RestoreDataCollector::Call::operator=(Call&&) =
    default;
RestoreDataCollector::Call::~Call() = default;

RestoreDataCollector::RestoreDataCollector() = default;
RestoreDataCollector::~RestoreDataCollector() = default;

void RestoreDataCollector::CaptureActiveDeskAsSavedDesk(
    GetDeskTemplateCallback callback,
    DeskTemplateType template_type,
    const std::string& template_name,
    aura::Window* root_window_to_show,
    AccountId current_account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto current_serial = serial_++;
  auto emplace_result = calls_.emplace(current_serial, Call{});
  DCHECK(emplace_result.second);
  Call& call = emplace_result.first->second;

  call.root_window_to_show = root_window_to_show;
  call.template_type = template_type;
  call.template_name = template_name;
  // Lacros profile IDs cannot be transferred between devices and is therefore
  // only enabled for save & recall (which is not synced between devices).
  if (template_type == DeskTemplateType::kSaveAndRecall &&
      chromeos::features::IsDeskProfilesEnabled()) {
    call.lacros_profile_id =
        DesksController::Get()->active_desk()->lacros_profile_id();
  }
  auto* window_manager = MultiUserWindowManagerImpl::Get();
  auto* const shell = Shell::Get();
  auto mru_windows =
      shell->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  auto* delegate = shell->saved_desk_delegate();
  bool has_supported_apps = false;
  for (aura::Window* window : mru_windows) {
    // Skip transient windows without reporting.
    if (wm::GetTransientParent(window)) {
      continue;
    }

    if (template_type == DeskTemplateType::kFloatingWorkspace) {
      // Filter the windows by profile ID associated with each window. Only save
      // the windows that are attached to the primary profile ID. For lacros,
      // the window profile id is non-zero. We can skip this check if it's on
      // ash.
      auto* desk_profile_delegate = Shell::Get()->GetDeskProfilesDelegate();
      CHECK(desk_profile_delegate);
      const uint64_t primary_profile_id =
          desk_profile_delegate->GetPrimaryProfileId();
      if (window->GetProperty(ash::kLacrosProfileId) != 0 &&
          window->GetProperty(ash::kLacrosProfileId) != primary_profile_id) {
        continue;
      }
    }

    // If `window_manager` is not nullptr, then we have a multi profile
    // session. We need to make sure that the windows we are capturing belongs
    // to the owner of the current active session.
    if (window_manager) {
      // Skip windows that belong to another profile user.
      const AccountId& window_owner = window_manager->GetWindowOwner(window);
      if (window_owner.is_valid() && current_account_id != window_owner) {
        continue;
      }
    }

    if (!delegate->IsWindowSupportedForSavedDesk(window)) {
      call.unsupported_apps.push_back(window);
      if (!delegate->IsWindowPersistable(window)) {
        ++call.non_persistable_window_count;
      }
      continue;
    }

    // Skip windows that do not associate with a full restore app id.
    const std::string app_id = saved_desk_util::GetAppId(window);
    if (!Shell::Get()
             ->overview_controller()
             ->disable_app_id_check_for_saved_desks() &&
        app_id.empty()) {
      call.unsupported_apps.push_back(window);
      continue;
    }
    has_supported_apps = true;

    std::unique_ptr<app_restore::WindowInfo> window_info =
        BuildWindowInfo(window, /*activation_index=*/std::nullopt, mru_windows);

    // Clear the desk ID and uuid in the WindowInfo that is to be stored in
    // the template. They will be set to the newly created desk when
    // launching.
    window_info->desk_id.reset();
    window_info->desk_guid = base::Uuid();

    ++call.pending_request_count;
    delegate->GetAppLaunchDataForSavedDesk(
        window, base::BindOnce(&RestoreDataCollector::OnAppLaunchDataReceived,
                               base::Unretained(this), current_serial, app_id,
                               std::move(window_info)));
  }

  // Do not create a saved desk if the desk is empty or only contains
  // unsupported apps.
  if (!has_supported_apps) {
    calls_.erase(current_serial);
    std::move(callback).Run(nullptr);
    return;
  }

  if (root_window_to_show) {
    window_tracker_.Add(root_window_to_show);
  }
  call.callback = std::move(callback);

  // If all requests in the loop above returned data synchronously, then we
  // have no pending requests and send the data right away.  Otherwise it will
  // be sent after the last pending request is handled.
  if (call.pending_request_count == 0) {
    SendDeskTemplate(current_serial);
  }
}

void RestoreDataCollector::OnAppLaunchDataReceived(
    uint32_t serial,
    const std::string& app_id,
    std::unique_ptr<app_restore::WindowInfo> window_info,
    std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto call_it = calls_.find(serial);
  DCHECK(call_it != calls_.end());
  Call& call = call_it->second;

  DCHECK(call.data);
  DCHECK_GT(call.pending_request_count, 0u);

  --call.pending_request_count;

  // nullptr means that this app does not have any data to save.
  if (app_launch_info) {
    const int32_t window_id = *app_launch_info->window_id;
    call.data->AddAppLaunchInfo(std::move(app_launch_info));
    call.data->ModifyWindowInfo(app_id, window_id, *window_info);
  }
  // Null callback here means that the loop in
  // `CaptureActiveDeskAsSavedDesk()` has not yet finished polling the
  // windows.  Non-zero pending request count means that some of preceding
  // requests were asynchronous.
  if (call.pending_request_count == 0 && !call.callback.is_null()) {
    SendDeskTemplate(serial);
  }
}

void RestoreDataCollector::SendDeskTemplate(uint32_t serial) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto call_it = calls_.find(serial);
  DCHECK(call_it != calls_.end());
  Call& call = call_it->second;

  auto desk_template = std::make_unique<DeskTemplate>(
      base::Uuid::GenerateRandomV4(), DeskTemplateSource::kUser,
      call.template_name, base::Time::Now(), call.template_type);
  desk_template->set_desk_restore_data(std::move(call.data));
  if (call.lacros_profile_id) {
    desk_template->set_lacros_profile_id(call.lacros_profile_id);
  }

  if (!call.unsupported_apps.empty() &&
      Shell::Get()->overview_controller()->InOverviewSession()) {
    // The ideal root window may have gone by now.  In that case fall back to
    // the primary root one.
    auto* root_window_to_show = call.root_window_to_show.get();
    if (root_window_to_show && window_tracker_.Contains(root_window_to_show)) {
      window_tracker_.Remove(root_window_to_show);
    } else {
      root_window_to_show = Shell::Get()->GetPrimaryRootWindow();
    }

    // There were some unsupported apps in the active desk so open up a dialog
    // to let the user know. The dialog controller should always be available
    // here since we have already determined that we are in overview mode.
    auto* dialog_controller = saved_desk_util::GetSavedDeskDialogController();
    DCHECK(dialog_controller);
    dialog_controller->ShowUnsupportedAppsDialog(
        root_window_to_show, call.unsupported_apps,
        call.non_persistable_window_count, std::move(call.callback),
        std::move(desk_template));
  } else {
    std::move(call.callback).Run(std::move(desk_template));
  }

  calls_.erase(call_it);
}

}  // namespace ash

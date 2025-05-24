// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_app_single_restore_handler.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler_factory.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/services/app_service/public/cpp/features.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/window/caption_button_layout_constants.h"

namespace ash::app_restore {

namespace {

bool IsAppReadyForLaunch(Profile* profile, const std::string& app_id) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile);
  return prefs && prefs->IsAbleToBeLaunched(app_id);
}

float GetDisplayScaleFactor(int64_t display_id) {
  auto* screen = display::Screen::GetScreen();
  float scale_factor = 1;
  if (screen) {
    scale_factor =
        display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
    for (auto disp : display::Screen::GetScreen()->GetAllDisplays()) {
      if (disp.id() == display_id)
        scale_factor = disp.device_scale_factor();
    }
  }
  return scale_factor;
}

}  // namespace

ArcAppSingleRestoreHandler::ArcAppSingleRestoreHandler() {
  observation_.Observe(full_restore::ArcGhostWindowHandler::Get());
}

ArcAppSingleRestoreHandler::~ArcAppSingleRestoreHandler() = default;

void ArcAppSingleRestoreHandler::LaunchGhostWindowWithApp(
    Profile* profile,
    const std::string& app_id,
    apps::IntentPtr intent,
    int event_flags,
    arc::GhostWindowType window_type,
    arc::mojom::WindowInfoPtr window_info) {
  // Activate ARC in case still not active. ArcSessionManager may null in test
  // env.
  if (arc::ArcSessionManager::Get()) {
    arc::ArcSessionManager::Get()->AllowActivation(
        arc::ArcSessionManager::AllowActivationReason::kRestoreApps);
  }

  // The ghost window and corresponding shelf item need to be added after ash
  // shelf ready.
  if (!is_shelf_ready_) {
    not_ready_callback_ = base::BindOnce(
        &ArcAppSingleRestoreHandler::LaunchGhostWindowWithApp,
        weak_ptr_factory_.GetWeakPtr(), profile, app_id, std::move(intent),
        event_flags, window_type, std::move(window_info));
    return;
  }

  DCHECK(profile);
  profile_ = profile;

  // For each single restore handler, the LaunchApp should be only called once.
  DCHECK(!app_id_.has_value());
  app_id_ = app_id;
  intent_ = std::move(intent);
  event_flags_ = event_flags;

  // Unit test use injected window handler.
  if (!ghost_window_handler_) {
    ghost_window_handler_ =
        AppRestoreArcTaskHandlerFactory::GetForProfile(profile)
            ->window_handler();
  }
  DCHECK(ghost_window_handler_);

  // Fill restore data by launch parameter to reuse full restore related
  // functions.
  ::app_restore::AppRestoreData restore_data;
  restore_data.window_info.current_bounds = window_info->bounds;
  restore_data.window_info.arc_extra_info = {.bounds_in_root =
                                                 window_info->bounds};

  // TODO: Remove this workaround.
  // Currently `ArcGhostWindowHandler::LaunchArcGhostWindow` assume all launch
  // bounds is from "recording data" in ash wm side, so it will reduce the top
  // caption bar size when launch ghost window. However, if here use the bounds
  // from Android side, the bounds should be added a "caption" size.
  if (restore_data.window_info.arc_extra_info->bounds_in_root.has_value()) {
    restore_data.window_info.arc_extra_info->bounds_in_root->Inset(
        gfx::Insets().set_top(
            -views::GetCaptionButtonLayoutSize(
                 views::CaptionButtonLayoutSize::kNonBrowserCaption)
                 .height()));
  }

  restore_data.window_info.window_state_type =
      static_cast<chromeos::WindowStateType>(window_info->state);
  restore_data.event_flag = event_flags;
  restore_data.display_id = window_info->display_id;

  // Even if the full restore is not enabled, still assign a session id to the
  // window in case these ghost window conflict.
  if (window_info->window_id == -1) {
    window_info->window_id = ::app_restore::CreateArcSessionId();
  }
  window_id_ = window_info->window_id;

  // Save the launch parameters for send launch request when app ready.
  window_info_ = std::make_unique<apps::WindowInfo>();
  window_info_->window_id = window_info->window_id;

  // Scale window bounds to ARC display unit.
  if (window_info->bounds.has_value()) {
    window_info_->bounds =
        gfx::ScaleToRoundedRect(window_info->bounds.value(),
                                GetDisplayScaleFactor(window_info->display_id));
  }
  window_info_->display_id = window_info->display_id;
  window_info_->state = window_info->state;

  ghost_window_handler_->LaunchArcGhostWindow(app_id, window_info->window_id,
                                              &restore_data);
  ghost_window_handler_->UpdateArcGhostWindowType(window_info->window_id,
                                                  window_type);

  // TODO: Add initial launch type on `LaunchArcGhostWindow`, rather update ARC
  // app states manually here.
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs) {
    auto app_info = prefs->GetApp(app_id);
    if (app_info) {
      ghost_window_handler_->OnAppStatesUpdate(app_id, app_info->ready,
                                               app_info->need_fixup);
    }
  }

  if (IsAppReadyForLaunch(profile_, app_id)) {
    // SendAppLaunchRequestToARC will reset `app_id_` after request sent.
    SendAppLaunchRequestToARC();
  }
}

bool ArcAppSingleRestoreHandler::IsAppPendingRestore(
    const std::string& app_id) const {
  return app_id_ && app_id == app_id_.value() && !is_cancelled_;
}

void ArcAppSingleRestoreHandler::OnShelfReady() {
  if (!not_ready_callback_.is_null()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(not_ready_callback_));
  }
  is_shelf_ready_ = true;
}

void ArcAppSingleRestoreHandler::OnWindowCloseRequested(int window_id) {
  if (window_id != window_id_)
    return;
  is_cancelled_ = true;
}

void ArcAppSingleRestoreHandler::OnAppStatesUpdate(const std::string& app_id,
                                                   bool ready,
                                                   bool need_fixup) {
  if (!app_id_ || app_id_.value() != app_id)
    return;
  // Update ARC app states immediately, since the app states may already
  // changed from original state.
  if (!is_cancelled_ && ready && !need_fixup)
    SendAppLaunchRequestToARC();
}

void ArcAppSingleRestoreHandler::OnGhostWindowHandlerDestroy() {
  observation_.Reset();
}

void ArcAppSingleRestoreHandler::SendAppLaunchRequestToARC() {
  if (!app_id_.has_value())
    return;

  DCHECK(profile_);
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  DCHECK(proxy);

  // TODO(sstan): Add new launch source.
  if (intent_) {
    proxy->LaunchAppWithIntent(app_id_.value(), ui::EF_NONE, std::move(intent_),
                               apps::LaunchSource::kFromFullRestore,
                               std::move(window_info_), base::DoNothing());
  } else {
    proxy->Launch(app_id_.value(), ui::EF_NONE,
                  apps::LaunchSource::kFromFullRestore,
                  std::move(window_info_));
  }

  // Remove app_id_ to make sure it only be called once for each app_id.
  app_id_.reset();
}

}  // namespace ash::app_restore

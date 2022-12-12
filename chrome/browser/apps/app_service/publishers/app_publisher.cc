// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/app_publisher.h"

#include "base/notreached.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "components/services/app_service/public/cpp/capability_access.h"

namespace apps {

AppPublisher::AppPublisher(AppServiceProxy* proxy) : proxy_(proxy) {
  DCHECK(proxy);
}

AppPublisher::~AppPublisher() = default;

// static
AppPtr AppPublisher::MakeApp(AppType app_type,
                             const std::string& app_id,
                             Readiness readiness,
                             const std::string& name,
                             InstallReason install_reason,
                             InstallSource install_source) {
  auto app = std::make_unique<App>(app_type, app_id);
  app->readiness = readiness;
  app->name = name;
  app->short_name = name;

  app->install_reason = install_reason;
  app->install_source = install_source;

  app->is_platform_app = false;
  app->recommendable = true;
  app->searchable = true;
  app->paused = false;

  return app;
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
void AppPublisher::RegisterPublisher(AppType app_type) {
  proxy_->RegisterPublisher(app_type, this);
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AppPublisher::GetCompressedIconData(const std::string& app_id,
                                         int32_t size_in_dip,
                                         ui::ResourceScaleFactor scale_factor,
                                         LoadIconCallback callback) {
  std::move(callback).Run(std::make_unique<IconValue>());
}
#endif

void AppPublisher::LaunchAppWithFiles(const std::string& app_id,
                                      int32_t event_flags,
                                      LaunchSource launch_source,
                                      std::vector<base::FilePath> file_paths) {
  NOTIMPLEMENTED();
}

void AppPublisher::LaunchAppWithIntent(const std::string& app_id,
                                       int32_t event_flags,
                                       IntentPtr intent,
                                       LaunchSource launch_source,
                                       WindowInfoPtr window_info,
                                       LaunchCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(LaunchResult(State::FAILED));
}

void AppPublisher::SetPermission(const std::string& app_id,
                                 PermissionPtr permission) {
  NOTIMPLEMENTED();
}

void AppPublisher::Uninstall(const std::string& app_id,
                             UninstallSource uninstall_source,
                             bool clear_site_data,
                             bool report_abuse) {
  LOG(ERROR) << "Uninstall failed, could not remove the app with id " << app_id;
}

void AppPublisher::PauseApp(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void AppPublisher::UnpauseApp(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void AppPublisher::StopApp(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void AppPublisher::GetMenuModel(const std::string& app_id,
                                MenuType menu_type,
                                int64_t display_id,
                                base::OnceCallback<void(MenuItems)> callback) {
  NOTIMPLEMENTED();
}

void AppPublisher::ExecuteContextMenuCommand(const std::string& app_id,
                                             int command_id,
                                             const std::string& shortcut_id,
                                             int64_t display_id) {
  NOTIMPLEMENTED();
}

void AppPublisher::OpenNativeSettings(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void AppPublisher::SetResizeLocked(const std::string& app_id, bool locked) {
  NOTIMPLEMENTED();
}

void AppPublisher::SetWindowMode(const std::string& app_id,
                                 WindowMode window_mode) {
  NOTIMPLEMENTED();
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
void AppPublisher::Publish(AppPtr app) {
  if (!proxy_) {
    NOTREACHED();
    return;
  }

  std::vector<AppPtr> apps;
  apps.push_back(std::move(app));
  proxy_->OnApps(std::move(apps), apps::AppType::kUnknown,
                 false /* should_notify_initialized */);
}

void AppPublisher::Publish(std::vector<AppPtr> apps,
                           AppType app_type,
                           bool should_notify_initialized) {
  if (!proxy_) {
    NOTREACHED();
    return;
  }
  proxy_->OnApps(std::move(apps), app_type, should_notify_initialized);
}

void AppPublisher::ModifyCapabilityAccess(
    const std::string& app_id,
    absl::optional<bool> accessing_camera,
    absl::optional<bool> accessing_microphone) {
  if (!accessing_camera.has_value() && !accessing_microphone.has_value()) {
    return;
  }

  std::vector<CapabilityAccessPtr> capability_accesses;
  auto capability_access = std::make_unique<CapabilityAccess>(app_id);
  capability_access->camera = accessing_camera;
  capability_access->microphone = accessing_microphone;
  capability_accesses.push_back(std::move(capability_access));
  proxy_->OnCapabilityAccesses(std::move(capability_accesses));
}
#endif

}  // namespace apps

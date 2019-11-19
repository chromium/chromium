// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/built_in_chromeos_apps.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_item.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

apps::mojom::AppPtr Convert(const app_list::InternalApp& internal_app) {
  if ((internal_app.app_id == nullptr) ||
      (internal_app.name_string_resource_id == 0) ||
      (internal_app.icon_resource_id <= 0)) {
    return apps::mojom::AppPtr();
  }
  apps::mojom::AppPtr app = apps::mojom::App::New();

  app->app_type = apps::mojom::AppType::kBuiltIn;
  app->app_id = internal_app.app_id;
  app->readiness = apps::mojom::Readiness::kReady;
  app->name = l10n_util::GetStringUTF8(internal_app.name_string_resource_id);
  app->short_name = app->name;
  if (internal_app.searchable_string_resource_id != 0) {
    app->additional_search_terms.push_back(
        l10n_util::GetStringUTF8(internal_app.searchable_string_resource_id));
  }

  app->icon_key = apps::mojom::IconKey::New(
      apps::mojom::IconKey::kDoesNotChangeOverTime,
      internal_app.icon_resource_id, apps::IconEffects::kNone);

  app->last_launch_time = base::Time();
  app->install_time = base::Time();

  app->install_source = apps::mojom::InstallSource::kSystem;

  app->is_platform_app = apps::mojom::OptionalBool::kFalse;
  app->recommendable = internal_app.recommendable
                           ? apps::mojom::OptionalBool::kTrue
                           : apps::mojom::OptionalBool::kFalse;
  app->searchable = internal_app.searchable ? apps::mojom::OptionalBool::kTrue
                                            : apps::mojom::OptionalBool::kFalse;
  app->show_in_launcher = internal_app.show_in_launcher
                              ? apps::mojom::OptionalBool::kTrue
                              : apps::mojom::OptionalBool::kFalse;
  app->show_in_search = internal_app.searchable
                            ? apps::mojom::OptionalBool::kTrue
                            : apps::mojom::OptionalBool::kFalse;
  app->show_in_management = apps::mojom::OptionalBool::kFalse;
  app->paused = apps::mojom::OptionalBool::kFalse;

  return app;
}

}  // namespace

namespace apps {

BuiltInChromeOsApps::BuiltInChromeOsApps(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile)
    : profile_(profile) {
  Initialize(app_service);
}

BuiltInChromeOsApps::~BuiltInChromeOsApps() = default;

void BuiltInChromeOsApps::FlushMojoCallsForTesting() {
  receiver_.FlushForTesting();
}

bool BuiltInChromeOsApps::hide_settings_app_for_testing_ = false;

// static
bool BuiltInChromeOsApps::SetHideSettingsAppForTesting(bool hide) {
  bool old_value = hide_settings_app_for_testing_;
  hide_settings_app_for_testing_ = hide;
  return old_value;
}

void BuiltInChromeOsApps::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  app_service->RegisterPublisher(receiver_.BindNewPipeAndPassRemote(),
                                 apps::mojom::AppType::kBuiltIn);
}

void BuiltInChromeOsApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  if (profile_) {
    // TODO(crbug.com/826982): move source of truth for built-in apps from
    // ui/app_list to here when the AppService feature is enabled by default.
    for (const auto& internal_app : app_list::GetInternalAppList(profile_)) {
      apps::mojom::AppPtr app = Convert(internal_app);
      if (!app.is_null()) {
        if (hide_settings_app_for_testing_ &&
            (internal_app.internal_app_name == BuiltInAppName::kSettings)) {
          app->show_in_search = apps::mojom::OptionalBool::kFalse;
        }
        apps.push_back(std::move(app));
      }
    }
  }
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps));

  // Unlike other apps::mojom::Publisher implementations, we don't need to
  // retain the subscriber (e.g. add it to a
  // mojo::RemoteSet<apps::mojom::Subscriber> subscribers_) after this
  // function returns. The list of built-in Chrome OS apps is fixed for the
  // lifetime of the Chrome OS session. There won't be any further updates.
}

void BuiltInChromeOsApps::LoadIcon(
    const std::string& app_id,
    apps::mojom::IconKeyPtr icon_key,
    apps::mojom::IconCompression icon_compression,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    LoadIconCallback callback) {
  constexpr bool is_placeholder_icon = false;
  if (icon_key &&
      (icon_key->resource_id != apps::mojom::IconKey::kInvalidResourceId)) {
    LoadIconFromResource(icon_compression, size_hint_in_dip,
                         icon_key->resource_id, is_placeholder_icon,
                         static_cast<IconEffects>(icon_key->icon_effects),
                         std::move(callback));
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void BuiltInChromeOsApps::Launch(const std::string& app_id,
                                 int32_t event_flags,
                                 apps::mojom::LaunchSource launch_source,
                                 int64_t display_id) {
  app_list::OpenInternalApp(app_id, profile_, event_flags);
}

void BuiltInChromeOsApps::LaunchAppWithIntent(
    const std::string& app_id,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    int64_t display_id) {
  NOTIMPLEMENTED();
}

void BuiltInChromeOsApps::SetPermission(const std::string& app_id,
                                        apps::mojom::PermissionPtr permission) {
  NOTIMPLEMENTED();
}

void BuiltInChromeOsApps::PromptUninstall(const std::string& app_id) {
  constexpr bool kClearSiteData = false;
  constexpr bool kReportAbuse = false;
  Uninstall(app_id, kClearSiteData, kReportAbuse);
}

void BuiltInChromeOsApps::Uninstall(const std::string& app_id,
                                    bool clear_site_data,
                                    bool report_abuse) {
  LOG(ERROR) << "Uninstall failed, could not remove built-in app with id "
             << app_id;
}

void BuiltInChromeOsApps::PauseApp(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void BuiltInChromeOsApps::UnpauseApps(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void BuiltInChromeOsApps::OpenNativeSettings(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void BuiltInChromeOsApps::OnPreferredAppSet(
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter,
    apps::mojom::IntentPtr intent) {
  NOTIMPLEMENTED();
}

}  // namespace apps

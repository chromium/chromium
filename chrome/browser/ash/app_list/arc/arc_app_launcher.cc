// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_launcher.h"

#include <memory>
#include <utility>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "ui/events/event_constants.h"

ArcAppLauncher::ArcAppLauncher(content::BrowserContext* context,
                               const std::string& app_id,
                               apps::IntentPtr launch_intent,
                               bool deferred_launch_allowed,
                               int64_t display_id,
                               apps::LaunchSource launch_source)
    : context_(context),
      app_id_(app_id),
      launch_intent_(std::move(launch_intent)),
      deferred_launch_allowed_(deferred_launch_allowed),
      display_id_(display_id),
      launch_source_(launch_source) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id_);
  if (!app_info ||
      !MaybeLaunchApp(app_id, *app_info, apps::Readiness::kUnknown)) {
    arc_app_list_prefs_observer_.Observe(prefs);
  }

  auto* profile = Profile::FromBrowserContext(context_);
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  app_registry_cache_observer_.Observe(
      &apps::AppServiceProxyFactory::GetForProfile(profile)
           ->AppRegistryCache());
}

ArcAppLauncher::~ArcAppLauncher() {
  if (!app_launched_) {
    VLOG(2) << "App " << app_id_ << "was not launched.";
  }
}

void ArcAppLauncher::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  MaybeLaunchApp(app_id, app_info, apps::Readiness::kUnknown);
}

void ArcAppLauncher::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  MaybeLaunchApp(app_id, app_info, apps::Readiness::kUnknown);
}

void ArcAppLauncher::OnArcAppListPrefsDestroyed() {
  arc_app_list_prefs_observer_.Reset();
}

void ArcAppLauncher::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() != app_id_ ||
      update.Readiness() != apps::Readiness::kReady) {
    return;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs);

  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id_);
  if (!app_info)
    return;

  MaybeLaunchApp(app_id_, *app_info, apps::Readiness::kReady);
}

void ArcAppLauncher::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

bool ArcAppLauncher::MaybeLaunchApp(const std::string& app_id,
                                    const ArcAppListPrefs::AppInfo& app_info,
                                    apps::Readiness readiness) {
  if (app_launched_)
    return true;

  if (app_id != app_id_ || (!app_info.ready && !deferred_launch_allowed_) ||
      app_info.suspended) {
    return false;
  }

  auto* profile = Profile::FromBrowserContext(context_);
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  if (readiness == apps::Readiness::kUnknown) {
    if (proxy->AppRegistryCache().GetAppType(app_id) ==
        apps::AppType::kUnknown) {
      return false;
    }

    proxy->AppRegistryCache().ForOneApp(
        app_id, [&readiness](const apps::AppUpdate& update) {
          readiness = update.Readiness();
        });
  }
  // Launch requests disabled by local settings should go through to App service
  // This is to ensure that the blocked app dialog is shown.
  if (readiness != apps::Readiness::kReady &&
      readiness != apps::Readiness::kDisabledByLocalSettings) {
    return false;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs && prefs->GetApp(app_id_));
  app_registry_cache_observer_.Reset();
  arc_app_list_prefs_observer_.Reset();

  if (launch_intent_) {
    proxy->LaunchAppWithIntent(
        app_id_, ui::EF_NONE, std::move(launch_intent_), launch_source_,
        std::make_unique<apps::WindowInfo>(display_id_), base::DoNothing());
  } else {
    proxy->Launch(app_id_, ui::EF_NONE, launch_source_,
                  std::make_unique<apps::WindowInfo>(display_id_));
  }

  app_launched_ = true;
  return true;
}

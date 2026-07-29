// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_launcher.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_service.h"
#include "components/services/app_service/public/cpp/app_service_registry.h"
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

  const auto& account_id = CHECK_DEREF(ash::AnnotatedAccountId::Get(context));
  auto& app_service =
      CHECK_DEREF(apps::AppServiceRegistry::Get()->Find(account_id));
  app_registry_cache_observer_.Observe(&app_service.AppRegistryCache());
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

  const auto& account_id = CHECK_DEREF(ash::AnnotatedAccountId::Get(context_));
  auto& app_service =
      CHECK_DEREF(apps::AppServiceRegistry::Get()->Find(account_id));

  if (readiness == apps::Readiness::kUnknown) {
    if (app_service.AppRegistryCache().GetAppType(app_id) ==
        apps::AppType::kUnknown) {
      return false;
    }

    app_service.AppRegistryCache().ForOneApp(
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

  // app_service.Launch / app_service.LaunchAppWithIntent can synchronously call
  // ShelfModel::ReplaceShelfItemDelegate (e.g. via the ARC deferred-launch
  // spinner path), which can destroy the
  // ArcPlaystoreShortcutShelfItemController that owns |this|. Guard the
  // trailing member write with a weak pointer.
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  if (launch_intent_) {
    app_service.LaunchAppWithIntent(
        app_id_, ui::EF_NONE, std::move(launch_intent_), launch_source_,
        std::make_unique<apps::WindowInfo>(display_id_), base::DoNothing());
  } else {
    app_service.Launch(app_id_, ui::EF_NONE, launch_source_,
                       std::make_unique<apps::WindowInfo>(display_id_));
  }

  if (!weak_this) {
    // |this| was destroyed during the synchronous launch chain.
    return true;
  }

  app_launched_ = true;
  return true;
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/uninstall_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/services/app_service/app_service_impl.h"
#include "chrome/services/app_service/public/cpp/instance_registry.h"
#include "chrome/services/app_service/public/cpp/intent_filter_util.h"
#include "chrome/services/app_service/public/cpp/intent_util.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/url_data_source.h"
#include "url/url_constants.h"

namespace apps {

AppServiceProxy::InnerIconLoader::InnerIconLoader(AppServiceProxy* host)
    : host_(host), overriding_icon_loader_for_testing_(nullptr) {}

apps::mojom::IconKeyPtr AppServiceProxy::InnerIconLoader::GetIconKey(
    const std::string& app_id) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->GetIconKey(app_id);
  }

  apps::mojom::IconKeyPtr icon_key;
  if (host_->app_service_.is_connected()) {
    host_->cache_.ForOneApp(app_id, [&icon_key](const apps::AppUpdate& update) {
      icon_key = update.IconKey();
    });
  }
  return icon_key;
}

std::unique_ptr<IconLoader::Releaser>
AppServiceProxy::InnerIconLoader::LoadIconFromIconKey(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IconKeyPtr icon_key,
    apps::mojom::IconCompression icon_compression,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->LoadIconFromIconKey(
        app_type, app_id, std::move(icon_key), icon_compression,
        size_hint_in_dip, allow_placeholder_icon, std::move(callback));
  }

  if (host_->app_service_.is_connected() && icon_key) {
    // TODO(crbug.com/826982): Mojo doesn't guarantee the order of messages,
    // so multiple calls to this method might not resolve their callbacks in
    // order. As per khmel@, "you may have race here, assume you publish change
    // for the app and app requested new icon. But new icon is not delivered
    // yet and you resolve old one instead. Now new icon arrives asynchronously
    // but you no longer notify the app or do?"
    host_->app_service_->LoadIcon(app_type, app_id, std::move(icon_key),
                                  icon_compression, size_hint_in_dip,
                                  allow_placeholder_icon, std::move(callback));
  } else {
    std::move(callback).Run(apps::mojom::IconValue::New());
  }
  return nullptr;
}

AppServiceProxy::AppServiceProxy(Profile* profile)
    : inner_icon_loader_(this),
      icon_coalescer_(&inner_icon_loader_),
      outer_icon_loader_(&icon_coalescer_,
                         apps::IconCache::GarbageCollectionPolicy::kEager),
      profile_(profile) {
  Initialize();
}

AppServiceProxy::~AppServiceProxy() = default;

void AppServiceProxy::ReInitializeForTesting(Profile* profile) {
  // Some test code creates a profile and profile-linked services, like the App
  // Service, before the profile is fully initialized. Such tests can call this
  // after full profile initialization to ensure the App Service implementation
  // has all of profile state it needs.
  app_service_.reset();
  profile_ = profile;
  Initialize();
}

void AppServiceProxy::Initialize() {
  if (!profile_) {
    return;
  }

  // We only initialize the App Service for regular or guest profiles. Non-guest
  // off-the-record profiles do not get an instance.
  if (profile_->IsOffTheRecord() && !profile_->IsGuestSession()) {
    return;
  }

  app_service_impl_ = std::make_unique<apps::AppServiceImpl>(
      content::BrowserContext::GetConnectorFor(profile_));
  app_service_impl_->BindReceiver(app_service_.BindNewPipeAndPassReceiver());

  if (app_service_.is_connected()) {
    // The AppServiceProxy is a subscriber: something that wants to be able to
    // list all known apps.
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber;
    receivers_.Add(this, subscriber.InitWithNewPipeAndPassReceiver());
    app_service_->RegisterSubscriber(std::move(subscriber), nullptr);

#if defined(OS_CHROMEOS)
    // The AppServiceProxy is also a publisher, of a variety of app types. That
    // responsibility isn't intrinsically part of the AppServiceProxy, but doing
    // that here, for each such app type, is as good a place as any.
    built_in_chrome_os_apps_ =
        std::make_unique<BuiltInChromeOsApps>(app_service_, profile_);
    crostini_apps_ = std::make_unique<CrostiniApps>(app_service_, profile_);
    extension_apps_ = std::make_unique<ExtensionApps>(
        app_service_, profile_, apps::mojom::AppType::kExtension,
        &instance_registry_);
    extension_web_apps_ = std::make_unique<ExtensionApps>(
        app_service_, profile_, apps::mojom::AppType::kWeb,
        &instance_registry_);

    // Asynchronously add app icon source, so we don't do too much work in the
    // constructor.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AppServiceProxy::AddAppIconSource,
                                  weak_ptr_factory_.GetWeakPtr(), profile_));
#endif  // OS_CHROMEOS
  }

  Observe(&cache_);
}

mojo::Remote<apps::mojom::AppService>& AppServiceProxy::AppService() {
  return app_service_;
}

apps::AppRegistryCache& AppServiceProxy::AppRegistryCache() {
  return cache_;
}

#if defined(OS_CHROMEOS)
apps::InstanceRegistry& AppServiceProxy::InstanceRegistry() {
  return instance_registry_;
}
#endif

apps::PreferredApps& AppServiceProxy::PreferredApps() {
  return preferred_apps_;
}

apps::mojom::IconKeyPtr AppServiceProxy::GetIconKey(const std::string& app_id) {
  return outer_icon_loader_.GetIconKey(app_id);
}

std::unique_ptr<apps::IconLoader::Releaser>
AppServiceProxy::LoadIconFromIconKey(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IconKeyPtr icon_key,
    apps::mojom::IconCompression icon_compression,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  return outer_icon_loader_.LoadIconFromIconKey(
      app_type, app_id, std::move(icon_key), icon_compression, size_hint_in_dip,
      allow_placeholder_icon, std::move(callback));
}

void AppServiceProxy::Launch(const std::string& app_id,
                             int32_t event_flags,
                             apps::mojom::LaunchSource launch_source,
                             int64_t display_id) {
  if (app_service_.is_connected()) {
    cache_.ForOneApp(app_id, [this, event_flags, launch_source,
                              display_id](const apps::AppUpdate& update) {
      if (update.Paused() == apps::mojom::OptionalBool::kTrue) {
        return;
      }
      RecordAppLaunch(update.AppId(), launch_source);
      app_service_->Launch(update.AppType(), update.AppId(), event_flags,
                           launch_source, display_id);
    });
  }
}

void AppServiceProxy::LaunchAppWithIntent(
    const std::string& app_id,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    int64_t display_id) {
  if (app_service_.is_connected()) {
    cache_.ForOneApp(app_id, [this, &intent, launch_source,
                              display_id](const apps::AppUpdate& update) {
      if (update.Paused() == apps::mojom::OptionalBool::kTrue) {
        return;
      }
      RecordAppLaunch(update.AppId(), launch_source);
      app_service_->LaunchAppWithIntent(update.AppType(), update.AppId(),
                                        std::move(intent), launch_source,
                                        display_id);
    });
  }
}

void AppServiceProxy::LaunchAppWithUrl(const std::string& app_id,
                                       GURL url,
                                       apps::mojom::LaunchSource launch_source,
                                       int64_t display_id) {
  LaunchAppWithIntent(app_id, apps_util::CreateIntentFromUrl(url),
                      launch_source, display_id);
}

void AppServiceProxy::SetPermission(const std::string& app_id,
                                    apps::mojom::PermissionPtr permission) {
  if (app_service_.is_connected()) {
    cache_.ForOneApp(
        app_id, [this, &permission](const apps::AppUpdate& update) {
          app_service_->SetPermission(update.AppType(), update.AppId(),
                                      std::move(permission));
        });
  }
}

void AppServiceProxy::Uninstall(const std::string& app_id,
                                gfx::NativeWindow parent_window) {
  if (app_service_.is_connected()) {
    cache_.ForOneApp(
        app_id, [this, parent_window](const apps::AppUpdate& update) {
          apps::mojom::IconKeyPtr icon_key = update.IconKey();
          uninstall_dialogs_.emplace(std::make_unique<UninstallDialog>(
              profile_, update.AppType(), update.AppId(), update.Name(),
              std::move(icon_key), this, parent_window,
              base::BindOnce(&AppServiceProxy::OnUninstallDialogClosed,
                             weak_ptr_factory_.GetWeakPtr(), update.AppType(),
                             update.AppId())));
        });
  }
}

void AppServiceProxy::OnUninstallDialogClosed(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    bool uninstall,
    bool clear_site_data,
    bool report_abuse,
    UninstallDialog* uninstall_dialog) {
  if (uninstall)
    app_service_->Uninstall(app_type, app_id, clear_site_data, report_abuse);

  DCHECK(uninstall_dialog);
  auto it = uninstall_dialogs_.find(uninstall_dialog);
  DCHECK(it != uninstall_dialogs_.end());
  uninstall_dialogs_.erase(it);
}

void AppServiceProxy::PauseApps(
    const std::map<std::string, PauseData>& pause_data) {
  if (!app_service_.is_connected())
    return;

  for (auto& data : pause_data) {
    apps::mojom::AppType app_type = cache_.GetAppType(data.first);
    constexpr bool kPaused = true;
    UpdatePausedStatus(app_type, data.first, kPaused);

    // TODO(crbug.com/1011235): Add the app running checking. If the app is not
    // running, don't create the pause dialog, pause the app directly.
    if (app_type != apps::mojom::AppType::kArc) {
      app_service_->PauseApp(app_type, data.first);
      continue;
    }

    cache_.ForOneApp(data.first, [this, &data](const apps::AppUpdate& update) {
      this->LoadIconForPauseDialog(update, data.second);
    });
  }
}

void AppServiceProxy::UnpauseApps(const std::set<std::string>& app_ids) {
  if (!app_service_.is_connected())
    return;

  for (auto& app_id : app_ids) {
    apps::mojom::AppType app_type = cache_.GetAppType(app_id);
    constexpr bool kPaused = false;
    UpdatePausedStatus(app_type, app_id, kPaused);

    app_service_->UnpauseApps(app_type, app_id);
  }
}

void AppServiceProxy::OnPauseDialogClosed(apps::mojom::AppType app_type,
                                          const std::string& app_id) {
  app_service_->PauseApp(app_type, app_id);
}

void AppServiceProxy::OpenNativeSettings(const std::string& app_id) {
  if (app_service_.is_connected()) {
    cache_.ForOneApp(app_id, [this](const apps::AppUpdate& update) {
      app_service_->OpenNativeSettings(update.AppType(), update.AppId());
    });
  }
}

void AppServiceProxy::FlushMojoCallsForTesting() {
  app_service_impl_->FlushMojoCallsForTesting();
#if defined(OS_CHROMEOS)
  built_in_chrome_os_apps_->FlushMojoCallsForTesting();
  crostini_apps_->FlushMojoCallsForTesting();
  extension_apps_->FlushMojoCallsForTesting();
  extension_web_apps_->FlushMojoCallsForTesting();
#endif
  receivers_.FlushForTesting();
}

apps::IconLoader* AppServiceProxy::OverrideInnerIconLoaderForTesting(
    apps::IconLoader* icon_loader) {
  apps::IconLoader* old =
      inner_icon_loader_.overriding_icon_loader_for_testing_;
  inner_icon_loader_.overriding_icon_loader_for_testing_ = icon_loader;
  return old;
}

void AppServiceProxy::ReInitializeCrostiniForTesting(Profile* profile) {
#if defined(OS_CHROMEOS)
  if (app_service_.is_connected()) {
    crostini_apps_->ReInitializeForTesting(app_service_, profile);
  }
#endif
}

std::vector<std::string> AppServiceProxy::GetAppIdsForUrl(const GURL& url) {
  return GetAppIdsForIntent(apps_util::CreateIntentFromUrl(url));
}

std::vector<std::string> AppServiceProxy::GetAppIdsForIntent(
    apps::mojom::IntentPtr intent) {
  std::vector<std::string> app_ids;
  if (app_service_.is_bound()) {
    cache_.ForEachApp([&app_ids, &intent](const apps::AppUpdate& update) {
      for (const auto& filter : update.IntentFilters()) {
        if (apps_util::IntentMatchesFilter(intent, filter)) {
          app_ids.push_back(update.AppId());
        }
      }
    });
  }
  return app_ids;
}

void AppServiceProxy::SetArcIsRegistered() {
#if defined(OS_CHROMEOS)
  if (arc_is_registered_) {
    return;
  }

  arc_is_registered_ = true;
  extension_apps_->ObserveArc();
  extension_web_apps_->ObserveArc();
#endif
}

void AppServiceProxy::AddPreferredApp(const std::string& app_id,
                                      const GURL& url) {
  AddPreferredApp(app_id, apps_util::CreateIntentFromUrl(url));
}

void AppServiceProxy::AddPreferredApp(const std::string& app_id,
                                      const apps::mojom::IntentPtr& intent) {
  auto intent_filter = FindBestMatchingFilter(intent);
  if (intent_filter) {
    preferred_apps_.AddPreferredApp(app_id, intent_filter);
    if (app_service_.is_connected()) {
      cache_.ForOneApp(app_id, [this, &intent_filter,
                                &intent](const apps::AppUpdate& update) {
        app_service_->AddPreferredApp(update.AppType(), update.AppId(),
                                      std::move(intent_filter),
                                      intent->Clone());
      });
    }
  }
}

void AppServiceProxy::AddAppIconSource(Profile* profile) {
  // Make the chrome://app-icon/ resource available.
  content::URLDataSource::Add(profile,
                              std::make_unique<apps::AppIconSource>(profile));
}

void AppServiceProxy::Shutdown() {
  uninstall_dialogs_.clear();

#if defined(OS_CHROMEOS)
  if (app_service_.is_connected()) {
    extension_apps_->Shutdown();
    extension_web_apps_->Shutdown();
  }
#endif  // OS_CHROMEOS
}

void AppServiceProxy::OnApps(std::vector<apps::mojom::AppPtr> deltas) {
  cache_.OnApps(std::move(deltas));
}

void AppServiceProxy::Clone(
    mojo::PendingReceiver<apps::mojom::Subscriber> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AppServiceProxy::OnPreferredAppSet(
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter) {
  preferred_apps_.AddPreferredApp(app_id, intent_filter);
}

void AppServiceProxy::InitializePreferredApps(base::Value preferred_apps) {
  preferred_apps_.Init(
      std::make_unique<base::Value>(std::move(preferred_apps)));
}

void AppServiceProxy::LoadIconForPauseDialog(const apps::AppUpdate& update,
                                             const PauseData& pause_data) {
  apps::mojom::IconKeyPtr icon_key = update.IconKey();
  constexpr bool kAllowPlaceholderIcon = false;
  constexpr int32_t kPauseIconSize = 48;
  LoadIconFromIconKey(
      update.AppType(), update.AppId(), std::move(icon_key),
      apps::mojom::IconCompression::kUncompressed, kPauseIconSize,
      kAllowPlaceholderIcon,
      base::BindOnce(&AppServiceProxy::OnLoadIconForPauseDialog,
                     weak_ptr_factory_.GetWeakPtr(), update.AppType(),
                     update.AppId(), update.Name(), pause_data));
}

void AppServiceProxy::OnLoadIconForPauseDialog(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    const std::string& app_name,
    const PauseData& pause_data,
    apps::mojom::IconValuePtr icon_value) {
  if (icon_value->icon_compression !=
      apps::mojom::IconCompression::kUncompressed) {
    OnPauseDialogClosed(app_type, app_id);
    return;
  }

  AppServiceProxy::CreatePauseDialog(
      app_name, icon_value->uncompressed, pause_data,
      base::BindOnce(&AppServiceProxy::OnPauseDialogClosed,
                     weak_ptr_factory_.GetWeakPtr(), app_type, app_id));
}

void AppServiceProxy::UpdatePausedStatus(apps::mojom::AppType app_type,
                                         const std::string& app_id,
                                         bool paused) {
  std::vector<apps::mojom::AppPtr> apps;
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type;
  app->app_id = app_id;
  app->paused = (paused) ? apps::mojom::OptionalBool::kTrue
                         : apps::mojom::OptionalBool::kFalse;
  apps.push_back(std::move(app));
  cache_.OnApps(std::move(apps));
}

void AppServiceProxy::OnAppUpdate(const apps::AppUpdate& update) {
  if (!update.ReadinessChanged() ||
      update.Readiness() != apps::mojom::Readiness::kUninstalledByUser) {
    return;
  }
  preferred_apps_.DeleteAppId(update.AppId());
}

void AppServiceProxy::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

apps::mojom::IntentFilterPtr AppServiceProxy::FindBestMatchingFilter(
    const apps::mojom::IntentPtr& intent) {
  apps::mojom::IntentFilterPtr best_matching_intent_filter;
  if (!app_service_.is_bound()) {
    return best_matching_intent_filter;
  }

  int best_match_level = apps_util::IntentFilterMatchLevel::kNone;
  cache_.ForEachApp([&intent, &best_match_level, &best_matching_intent_filter](
                        const apps::AppUpdate& update) {
    for (const auto& filter : update.IntentFilters()) {
      if (!apps_util::IntentMatchesFilter(intent, filter)) {
        continue;
      }
      auto match_level = apps_util::GetFilterMatchLevel(filter);
      if (match_level <= best_match_level) {
        continue;
      }
      best_matching_intent_filter = filter->Clone();
      best_match_level = match_level;
    }
  });
  return best_matching_intent_filter;
}

}  // namespace apps

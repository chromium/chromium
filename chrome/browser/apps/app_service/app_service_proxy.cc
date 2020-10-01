// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/app_service_impl.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/url_data_source.h"
#include "ui/display/types/display_constants.h"
#include "url/url_constants.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/apps/app_service/lacros_apps.h"
#include "chrome/browser/apps/app_service/uninstall_dialog.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_interface.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/supervised_user/grit/supervised_user_unscaled_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user.h"
#include "extensions/common/constants.h"
#endif

namespace apps {

namespace {

bool g_omit_built_in_apps_for_testing_ = false;
bool g_omit_plugin_vm_apps_for_testing_ = false;

}  // anonymous namespace

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
    apps::mojom::IconType icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->LoadIconFromIconKey(
        app_type, app_id, std::move(icon_key), icon_type, size_hint_in_dip,
        allow_placeholder_icon, std::move(callback));
  }

  if (host_->app_service_.is_connected() && icon_key) {
    // TODO(crbug.com/826982): Mojo doesn't guarantee the order of messages,
    // so multiple calls to this method might not resolve their callbacks in
    // order. As per khmel@, "you may have race here, assume you publish change
    // for the app and app requested new icon. But new icon is not delivered
    // yet and you resolve old one instead. Now new icon arrives asynchronously
    // but you no longer notify the app or do?"
    host_->app_service_->LoadIcon(app_type, app_id, std::move(icon_key),
                                  icon_type, size_hint_in_dip,
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

AppServiceProxy::~AppServiceProxy() {
#if defined(OS_CHROMEOS)
  AppRegistryCacheWrapper::Get().RemoveAppRegistryCache(&cache_);
#endif
}

void AppServiceProxy::ReInitializeForTesting(Profile* profile) {
  // Some test code creates a profile and profile-linked services, like the App
  // Service, before the profile is fully initialized. Such tests can call this
  // after full profile initialization to ensure the App Service implementation
  // has all of profile state it needs.
  app_service_.reset();
  profile_ = profile;
  is_using_testing_profile_ = true;
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

#if defined(OS_CHROMEOS)
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user) {
    cache_.SetAccountId(user->GetAccountId());
    AppRegistryCacheWrapper::Get().AddAppRegistryCache(user->GetAccountId(),
                                                       &cache_);
  }
#endif

  browser_app_launcher_ = std::make_unique<apps::BrowserAppLauncher>(profile_);

  app_service_impl_ = std::make_unique<apps::AppServiceImpl>(
      profile_->GetPath(),
      base::FeatureList::IsEnabled(features::kIntentHandlingSharing));
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
    if (!g_omit_built_in_apps_for_testing_) {
      built_in_chrome_os_apps_ =
          std::make_unique<BuiltInChromeOsApps>(app_service_, profile_);
    }
    crostini_apps_ = std::make_unique<CrostiniApps>(app_service_, profile_);
    extension_apps_ = std::make_unique<ExtensionAppsChromeOs>(
        app_service_, profile_, apps::mojom::AppType::kExtension,
        &instance_registry_);
    if (!g_omit_plugin_vm_apps_for_testing_) {
      plugin_vm_apps_ = std::make_unique<PluginVmApps>(app_service_, profile_);
    }
    if (chromeos::features::IsLacrosSupportEnabled()) {
      // LacrosApps uses LacrosManager, which is a singleton. Don't create an
      // instance of LacrosApps for the lock screen app profile, as we want to
      // maintain a single instance of LacrosApps.
      // TODO(jamescook): Multiprofile support. Consider switching to observers.
      if (!chromeos::ProfileHelper::IsLockScreenAppProfile(profile_)) {
        lacros_apps_ = std::make_unique<LacrosApps>(app_service_);
      }
    }
    if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions)) {
      web_apps_ = std::make_unique<WebAppsChromeOs>(app_service_, profile_,
                                                    &instance_registry_);
    } else {
      extension_web_apps_ = std::make_unique<ExtensionAppsChromeOs>(
          app_service_, profile_, apps::mojom::AppType::kWeb,
          &instance_registry_);
    }
    borealis_apps_ = std::make_unique<BorealisApps>(app_service_, profile_);
#else
    if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions)) {
      web_apps_ = std::make_unique<WebApps>(app_service_, profile_);
    } else {
      extension_web_apps_ = std::make_unique<ExtensionApps>(
          app_service_, profile_, apps::mojom::AppType::kWeb);
    }
    extension_apps_ = std::make_unique<ExtensionApps>(
        app_service_, profile_, apps::mojom::AppType::kExtension);
#endif

    // Asynchronously add app icon source, so we don't do too much work in the
    // constructor.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AppServiceProxy::AddAppIconSource,
                                  weak_ptr_factory_.GetWeakPtr(), profile_));
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

BrowserAppLauncher* AppServiceProxy::BrowserAppLauncher() {
  return browser_app_launcher_.get();
}

apps::PreferredAppsList& AppServiceProxy::PreferredApps() {
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
    apps::mojom::IconType icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  return outer_icon_loader_.LoadIconFromIconKey(
      app_type, app_id, std::move(icon_key), icon_type, size_hint_in_dip,
      allow_placeholder_icon, std::move(callback));
}

void AppServiceProxy::Launch(const std::string& app_id,
                             int32_t event_flags,
                             apps::mojom::LaunchSource launch_source,
                             int64_t display_id) {
  if (app_service_.is_connected()) {
    cache_.ForOneApp(app_id, [this, event_flags, launch_source,
                              display_id](const apps::AppUpdate& update) {
#if defined(OS_CHROMEOS)
      if (MaybeShowLaunchPreventionDialog(update)) {
        return;
      }
#endif
      // Don't record system apps metric here, they are handled in
      // LaunchSystemWebApp.
      base::Optional<web_app::SystemAppType> system_app_type =
          web_app::GetSystemWebAppTypeForAppId(profile_, update.AppId());
      if (!system_app_type) {
        RecordAppLaunch(update.AppId(), launch_source);
      }
      app_service_->Launch(update.AppType(), update.AppId(), event_flags,
                           launch_source, display_id);
    });
  }
}

void AppServiceProxy::LaunchAppWithFiles(
    const std::string& app_id,
    apps::mojom::LaunchContainer container,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::FilePathsPtr file_paths) {
  if (app_service_.is_connected()) {
    cache_.ForOneApp(app_id, [this, container, event_flags, launch_source,
                              &file_paths](const apps::AppUpdate& update) {
#if defined(OS_CHROMEOS)
      if (MaybeShowLaunchPreventionDialog(update)) {
        return;
      }
#endif
      // TODO(crbug/1117655): Presently, app launch metrics are recorded in the
      // caller. We should record them here, with the same SWA logic as
      // AppServiceProxy::Launch. There is an if statement to detect launches
      // from the file manager in LaunchSystemWebApp that should be removed at
      // the same time.
      app_service_->LaunchAppWithFiles(update.AppType(), update.AppId(),
                                       container, event_flags, launch_source,
                                       std::move(file_paths));
    });
  }
}

void AppServiceProxy::LaunchAppWithFileUrls(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    const std::vector<GURL>& file_urls,
    const std::vector<std::string>& mime_types) {
  LaunchAppWithIntent(
      app_id, event_flags,
      apps_util::CreateShareIntentFromFiles(file_urls, mime_types),
      launch_source, display::kDefaultDisplayId);
}

void AppServiceProxy::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    int64_t display_id) {
  if (app_service_.is_connected()) {
    cache_.ForOneApp(app_id, [this, event_flags, &intent, launch_source,
                              display_id](const apps::AppUpdate& update) {
#if defined(OS_CHROMEOS)
      if (MaybeShowLaunchPreventionDialog(update)) {
        return;
      }
#endif
      base::Optional<web_app::SystemAppType> system_app_type =
          web_app::GetSystemWebAppTypeForAppId(profile_, update.AppId());
      if (!system_app_type) {
        // Don't record system apps metric here, they are handled in
        // LaunchSystemWebApp.
        RecordAppLaunch(update.AppId(), launch_source);
      }
      app_service_->LaunchAppWithIntent(update.AppType(), update.AppId(),
                                        event_flags, std::move(intent),
                                        launch_source, display_id);
    });
  }
}

void AppServiceProxy::LaunchAppWithUrl(const std::string& app_id,
                                       int32_t event_flags,
                                       GURL url,
                                       apps::mojom::LaunchSource launch_source,
                                       int64_t display_id) {
  LaunchAppWithIntent(app_id, event_flags, apps_util::CreateIntentFromUrl(url),
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
#if defined(OS_CHROMEOS)
  UninstallImpl(app_id, parent_window, base::DoNothing());
#else
  // On non-ChromeOS, publishers run the remove dialog.
  apps::mojom::AppType app_type = cache_.GetAppType(app_id);
  if (app_type == apps::mojom::AppType::kWeb) {
    if (!base::FeatureList::IsEnabled(
            features::kDesktopPWAsWithoutExtensions)) {
      ExtensionApps::UninstallImpl(profile_, app_id, parent_window);
    } else {
      WebApps::UninstallImpl(profile_, app_id, parent_window);
    }
  }
#endif
}

void AppServiceProxy::UninstallSilently(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source) {
  if (app_service_.is_connected()) {
    app_service_->Uninstall(cache_.GetAppType(app_id), app_id, uninstall_source,
                            /*clear_site_data=*/false, /*report_abuse=*/false);
  }
}

#if defined(OS_CHROMEOS)
void AppServiceProxy::PauseApps(
    const std::map<std::string, PauseData>& pause_data) {
  if (!app_service_.is_connected()) {
    return;
  }

  for (auto& data : pause_data) {
    apps::mojom::AppType app_type = cache_.GetAppType(data.first);
    if (app_type == apps::mojom::AppType::kUnknown) {
      continue;
    }

    cache_.ForOneApp(data.first, [this](const apps::AppUpdate& update) {
      if (update.Paused() != apps::mojom::OptionalBool::kTrue) {
        pending_pause_requests_.MaybeAddApp(update.AppId());
      }
    });

    // The app pause dialog can't be loaded for unit tests.
    if (!data.second.should_show_pause_dialog || is_using_testing_profile_) {
      app_service_->PauseApp(app_type, data.first);
      continue;
    }

    cache_.ForOneApp(data.first, [this, &data](const apps::AppUpdate& update) {
      LoadIconForDialog(
          update,
          base::BindOnce(&AppServiceProxy::OnLoadIconForPauseDialog,
                         weak_ptr_factory_.GetWeakPtr(), update.AppType(),
                         update.AppId(), update.Name(), data.second));
    });
  }
}

void AppServiceProxy::UnpauseApps(const std::set<std::string>& app_ids) {
  if (!app_service_.is_connected()) {
    return;
  }

  for (auto& app_id : app_ids) {
    apps::mojom::AppType app_type = cache_.GetAppType(app_id);
    if (app_type == apps::mojom::AppType::kUnknown) {
      continue;
    }

    pending_pause_requests_.MaybeRemoveApp(app_id);
    app_service_->UnpauseApps(app_type, app_id);
  }
}
#endif  // OS_CHROMEOS

void AppServiceProxy::StopApp(const std::string& app_id) {
  if (!app_service_.is_connected()) {
    return;
  }
  apps::mojom::AppType app_type = cache_.GetAppType(app_id);
  app_service_->StopApp(app_type, app_id);
}

void AppServiceProxy::GetMenuModel(
    const std::string& app_id,
    apps::mojom::MenuType menu_type,
    int64_t display_id,
    apps::mojom::Publisher::GetMenuModelCallback callback) {
  if (!app_service_.is_connected()) {
    return;
  }

  apps::mojom::AppType app_type = cache_.GetAppType(app_id);
  app_service_->GetMenuModel(app_type, app_id, menu_type, display_id,
                             std::move(callback));
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
  if (built_in_chrome_os_apps_)
    built_in_chrome_os_apps_->FlushMojoCallsForTesting();
  crostini_apps_->FlushMojoCallsForTesting();
  extension_apps_->FlushMojoCallsForTesting();
  if (plugin_vm_apps_)
    plugin_vm_apps_->FlushMojoCallsForTesting();
  if (lacros_apps_) {
    lacros_apps_->FlushMojoCallsForTesting();
  }
  if (web_apps_) {
    web_apps_->FlushMojoCallsForTesting();
  } else {
    extension_web_apps_->FlushMojoCallsForTesting();
  }
  if (borealis_apps_) {
    borealis_apps_->FlushMojoCallsForTesting();
  }
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

#if defined(OS_CHROMEOS)
void AppServiceProxy::ReInitializeCrostiniForTesting(Profile* profile) {
  if (app_service_.is_connected()) {
    crostini_apps_->ReInitializeForTesting(app_service_, profile);
  }
}

void AppServiceProxy::SetDialogCreatedCallbackForTesting(
    base::OnceClosure callback) {
  dialog_created_callback_ = std::move(callback);
}

void AppServiceProxy::UninstallForTesting(const std::string& app_id,
                                          gfx::NativeWindow parent_window,
                                          base::OnceClosure callback) {
  UninstallImpl(app_id, parent_window, std::move(callback));
}

#endif

std::vector<std::string> AppServiceProxy::GetAppIdsForUrl(
    const GURL& url,
    bool exclude_browsers) {
  auto intent_launch_info =
      GetAppsForIntent(apps_util::CreateIntentFromUrl(url), exclude_browsers);
  std::vector<std::string> app_ids;
  for (auto& entry : intent_launch_info) {
    app_ids.push_back(std::move(entry.app_id));
  }
  return app_ids;
}

std::vector<IntentLaunchInfo> AppServiceProxy::GetAppsForIntent(
    const apps::mojom::IntentPtr& intent,
    bool exclude_browsers) {
  std::vector<IntentLaunchInfo> intent_launch_info;
  if (apps_util::OnlyShareToDrive(intent) ||
      !apps_util::IsIntentValid(intent)) {
    return intent_launch_info;
  }

  if (app_service_.is_bound()) {
    cache_.ForEachApp([&intent_launch_info, &intent,
                       &exclude_browsers](const apps::AppUpdate& update) {
      if (update.Readiness() == apps::mojom::Readiness::kUninstalledByUser) {
        return;
      }
      std::set<std::string> existing_activities;
      for (const auto& filter : update.IntentFilters()) {
        if (exclude_browsers && apps_util::IsBrowserFilter(filter)) {
          continue;
        }
        if (apps_util::IntentMatchesFilter(intent, filter)) {
          IntentLaunchInfo entry;
          entry.app_id = update.AppId();
          std::string activity_label;
          if (filter->activity_label &&
              !filter->activity_label.value().empty()) {
            activity_label = filter->activity_label.value();
          } else {
            activity_label = update.Name();
          }
          if (base::Contains(existing_activities, activity_label)) {
            continue;
          }
          existing_activities.insert(activity_label);
          entry.activity_label = activity_label;
          entry.activity_name = filter->activity_name.value_or("");
          intent_launch_info.push_back(entry);
        }
      }
    });
  }
  return intent_launch_info;
}

std::vector<IntentLaunchInfo> AppServiceProxy::GetAppsForFiles(
    const std::vector<GURL>& filesystem_urls,
    const std::vector<std::string>& mime_types) {
  return GetAppsForIntent(
      apps_util::CreateShareIntentFromFiles(filesystem_urls, mime_types));
}

void AppServiceProxy::SetArcIsRegistered() {
#if defined(OS_CHROMEOS)
  if (arc_is_registered_) {
    return;
  }

  arc_is_registered_ = true;
  extension_apps_->ObserveArc();
  if (web_apps_) {
    web_apps_->ObserveArc();
  } else {
    extension_web_apps_->ObserveArc();
  }
#endif
}

void AppServiceProxy::AddPreferredApp(const std::string& app_id,
                                      const GURL& url) {
  AddPreferredApp(app_id, apps_util::CreateIntentFromUrl(url));
}

void AppServiceProxy::AddPreferredApp(const std::string& app_id,
                                      const apps::mojom::IntentPtr& intent) {
  // TODO(https://crbug.com/853604): Remove this and convert to a DCHECK
  // after finding out the root cause.
  if (app_id.empty()) {
    base::debug::DumpWithoutCrashing();
    return;
  }
  auto intent_filter = FindBestMatchingFilter(intent);
  if (!intent_filter) {
    return;
  }
  preferred_apps_.AddPreferredApp(app_id, intent_filter);
  if (app_service_.is_connected()) {
    constexpr bool kFromPublisher = false;
    app_service_->AddPreferredApp(cache_.GetAppType(app_id), app_id,
                                  std::move(intent_filter), intent->Clone(),
                                  kFromPublisher);
  }
}

void AppServiceProxy::AddAppIconSource(Profile* profile) {
  // Make the chrome://app-icon/ resource available.
  content::URLDataSource::Add(profile,
                              std::make_unique<apps::AppIconSource>(profile));
}

void AppServiceProxy::Shutdown() {
#if defined(OS_CHROMEOS)
  uninstall_dialogs_.clear();

  if (app_service_.is_connected()) {
    extension_apps_->Shutdown();
    if (web_apps_) {
      web_apps_->Shutdown();
    } else {
      extension_web_apps_->Shutdown();
    }
  }
#endif
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

void AppServiceProxy::OnPreferredAppRemoved(
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter) {
  preferred_apps_.DeletePreferredApp(app_id, intent_filter);
}

void AppServiceProxy::InitializePreferredApps(
    PreferredAppsList::PreferredApps preferred_apps) {
  preferred_apps_.Init(preferred_apps);
}

#if defined(OS_CHROMEOS)
void AppServiceProxy::UninstallImpl(const std::string& app_id,
                                    gfx::NativeWindow parent_window,
                                    base::OnceClosure callback) {
  if (!app_service_.is_connected()) {
    return;
  }

  cache_.ForOneApp(app_id, [this, parent_window,
                            &callback](const apps::AppUpdate& update) {
    apps::mojom::IconKeyPtr icon_key = update.IconKey();
    auto uninstall_dialog = std::make_unique<UninstallDialog>(
        profile_, update.AppType(), update.AppId(), update.Name(),
        std::move(icon_key), this, parent_window,
        base::BindOnce(&AppServiceProxy::OnUninstallDialogClosed,
                       weak_ptr_factory_.GetWeakPtr(), update.AppType(),
                       update.AppId()));
    uninstall_dialog->SetDialogCreatedCallbackForTesting(std::move(callback));
    uninstall_dialogs_.emplace(std::move(uninstall_dialog));
  });
}

void AppServiceProxy::OnUninstallDialogClosed(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    bool uninstall,
    bool clear_site_data,
    bool report_abuse,
    UninstallDialog* uninstall_dialog) {
  if (uninstall) {
    cache_.ForOneApp(app_id, RecordAppBounce);

    app_service_->Uninstall(app_type, app_id,
                            apps::mojom::UninstallSource::kUser,
                            clear_site_data, report_abuse);
  }

  DCHECK(uninstall_dialog);
  auto it = uninstall_dialogs_.find(uninstall_dialog);
  DCHECK(it != uninstall_dialogs_.end());
  uninstall_dialogs_.erase(it);
}

bool AppServiceProxy::MaybeShowLaunchPreventionDialog(
    const apps::AppUpdate& update) {
  if (update.AppId() == extension_misc::kChromeAppId) {
    return false;
  }

  // Return true, and load the icon for the app block dialog when the app
  // is blocked by policy.
  if (update.Readiness() == apps::mojom::Readiness::kDisabledByPolicy) {
    LoadIconForDialog(
        update, base::BindOnce(&AppServiceProxy::OnLoadIconForBlockDialog,
                               weak_ptr_factory_.GetWeakPtr(), update.Name()));
    return true;
  }

  // Return true, and load the icon for the app pause dialog when the app
  // is paused.
  if (update.Paused() == apps::mojom::OptionalBool::kTrue ||
      pending_pause_requests_.IsPaused(update.AppId())) {
    chromeos::app_time::AppTimeLimitInterface* app_limit =
        chromeos::app_time::AppTimeLimitInterface::Get(profile_);
    DCHECK(app_limit);
    auto time_limit =
        app_limit->GetTimeLimitForApp(update.AppId(), update.AppType());
    if (!time_limit.has_value()) {
      NOTREACHED();
      return true;
    }
    PauseData pause_data;
    pause_data.hours = time_limit.value().InHours();
    pause_data.minutes = time_limit.value().InMinutes() % 60;
    LoadIconForDialog(
        update, base::BindOnce(&AppServiceProxy::OnLoadIconForPauseDialog,
                               weak_ptr_factory_.GetWeakPtr(), update.AppType(),
                               update.AppId(), update.Name(), pause_data));
    return true;
  }

  // The app is not prevented from launching and we didn't show any dialog.
  return false;
}

void AppServiceProxy::LoadIconForDialog(
    const apps::AppUpdate& update,
    apps::mojom::Publisher::LoadIconCallback callback) {
  apps::mojom::IconKeyPtr icon_key = update.IconKey();
  constexpr bool kAllowPlaceholderIcon = false;
  constexpr int32_t kIconSize = 48;
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;

  // For browser tests, load the app icon, because there is no family link
  // logo for browser tests.
  //
  // For non_child profile, load the app icon, because the app is blocked by
  // admin.
  if (!dialog_created_callback_.is_null() || !profile_->IsChild()) {
    LoadIconFromIconKey(update.AppType(), update.AppId(), std::move(icon_key),
                        icon_type, kIconSize, kAllowPlaceholderIcon,
                        std::move(callback));
    return;
  }

  // Load the family link kite logo icon for the app pause dialog or the app
  // block dialog for the child profile.
  LoadIconFromResource(icon_type, kIconSize, IDR_SUPERVISED_USER_ICON,
                       kAllowPlaceholderIcon, IconEffects::kNone,
                       std::move(callback));
}

void AppServiceProxy::OnLoadIconForBlockDialog(
    const std::string& app_name,
    apps::mojom::IconValuePtr icon_value) {
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  if (icon_value->icon_type != icon_type) {
    return;
  }

  AppServiceProxy::CreateBlockDialog(app_name, icon_value->uncompressed,
                                     profile_);

  // For browser tests, call the dialog created callback to stop the run loop.
  if (!dialog_created_callback_.is_null()) {
    std::move(dialog_created_callback_).Run();
  }
}

void AppServiceProxy::OnLoadIconForPauseDialog(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    const std::string& app_name,
    const PauseData& pause_data,
    apps::mojom::IconValuePtr icon_value) {
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  if (icon_value->icon_type != icon_type) {
    OnPauseDialogClosed(app_type, app_id);
    return;
  }

  AppServiceProxy::CreatePauseDialog(
      app_type, app_name, icon_value->uncompressed, pause_data,
      base::BindOnce(&AppServiceProxy::OnPauseDialogClosed,
                     weak_ptr_factory_.GetWeakPtr(), app_type, app_id));

  // For browser tests, call the dialog created callback to stop the run loop.
  if (!dialog_created_callback_.is_null()) {
    std::move(dialog_created_callback_).Run();
  }
}

void AppServiceProxy::OnPauseDialogClosed(apps::mojom::AppType app_type,
                                          const std::string& app_id) {
  bool should_pause_app = pending_pause_requests_.IsPaused(app_id);
  if (!should_pause_app) {
    cache_.ForOneApp(
        app_id, [&should_pause_app](const apps::AppUpdate& update) {
          if (update.Paused() == apps::mojom::OptionalBool::kTrue) {
            should_pause_app = true;
          }
        });
  }
  if (should_pause_app) {
    app_service_->PauseApp(app_type, app_id);
  }
}
#endif  // OS_CHROMEOS

void AppServiceProxy::OnAppUpdate(const apps::AppUpdate& update) {
#if defined(OS_CHROMEOS)
  if ((update.PausedChanged() &&
       update.Paused() == apps::mojom::OptionalBool::kTrue) ||
      (update.ReadinessChanged() &&
       update.Readiness() == apps::mojom::Readiness::kUninstalledByUser)) {
    pending_pause_requests_.MaybeRemoveApp(update.AppId());
  }
#endif  // OS_CHROMEOS

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

ScopedOmitBuiltInAppsForTesting::ScopedOmitBuiltInAppsForTesting()
    : previous_omit_built_in_apps_for_testing_(
          g_omit_built_in_apps_for_testing_) {
  g_omit_built_in_apps_for_testing_ = true;
}

ScopedOmitBuiltInAppsForTesting::~ScopedOmitBuiltInAppsForTesting() {
  g_omit_built_in_apps_for_testing_ = previous_omit_built_in_apps_for_testing_;
}

ScopedOmitPluginVmAppsForTesting::ScopedOmitPluginVmAppsForTesting()
    : previous_omit_plugin_vm_apps_for_testing_(
          g_omit_plugin_vm_apps_for_testing_) {
  g_omit_plugin_vm_apps_for_testing_ = true;
}

ScopedOmitPluginVmAppsForTesting::~ScopedOmitPluginVmAppsForTesting() {
  g_omit_plugin_vm_apps_for_testing_ =
      previous_omit_plugin_vm_apps_for_testing_;
}

}  // namespace apps

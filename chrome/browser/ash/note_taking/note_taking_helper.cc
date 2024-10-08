// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/note_taking/note_taking_helper.h"

#include <stddef.h>

#include <atomic>
#include <map>
#include <ostream>
#include <utility>

#include "apps/launcher.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/mojom/file_system.mojom-forward.h"
#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/mojom/intent_common.mojom-forward.h"
#include "ash/components/arc/mojom/intent_common.mojom-shared.h"
#include "ash/components/arc/mojom/intent_common.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/stylus_utils.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/lock_screen_apps/lock_screen_apps.h"
#include "chrome/browser/ash/note_taking/note_taking_controller_client.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/action_handlers_handler.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "ui/display/display.h"
#include "ui/display/util/display_util.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

namespace ash {
namespace {

namespace app_runtime = ::extensions::api::app_runtime;

// Pointer to singleton instance.
NoteTakingHelper* g_helper = nullptr;

// Allowed note-taking app IDs. These will be treated as note-taking apps
// regardless of the app metadata, and will be shown in this order at the top of
// the list of note-taking apps.
const char* const kDefaultAllowedAppIds[] = {
    web_app::kCursiveAppId,
    NoteTakingHelper::kDevKeepExtensionId,
    NoteTakingHelper::kProdKeepExtensionId,
    NoteTakingHelper::kNoteTakingWebAppIdTest,
};

// Types of App Service apps that support note taking. Note that Note Taking
// Chrome Apps are not supported in Lacros, so kStandaloneBrowserChromeApp is
// not included.
// TODO (crbug.com/1336120): Add Android here.
const apps::AppType kNoteTakingAppTypes[] = {apps::AppType::kWeb,
                                             apps::AppType::kChromeApp};

// Returns whether `app_id` looks like it's probably an Android package name
// rather than a Chrome extension ID or web app ID.
bool LooksLikeAndroidPackageName(const std::string& app_id) {
  // Android package names are required to contain at least one period (see
  // validateName() in PackageParser.java), while Chrome extension IDs and web
  // app IDs contain only characters in [a-p].
  return base::Contains(app_id, '.');
}

bool IsInstalledApp(const std::string& app_id, Profile* profile) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return false;
  auto& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();

  bool result = false;
  cache.ForOneApp(app_id, [&result](const apps::AppUpdate& update) {
    if (apps_util::IsInstalled(update.Readiness())) {
      result = true;
    }
  });
  return result;
}

bool IsInstalledWebApp(const std::string& app_id, Profile* profile) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return false;
  auto& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();

  bool result = false;
  cache.ForOneApp(app_id, [&result](const apps::AppUpdate& update) {
    if (apps_util::IsInstalled(update.Readiness()) &&
        update.AppType() == apps::AppType::kWeb) {
      result = true;
    }
  });
  return result;
}

// Creates a new Mojo IntentInfo struct for launching an Android note-taking app
// with an optional ClipData URI.
arc::mojom::IntentInfoPtr CreateIntentInfo(const GURL& clip_data_uri) {
  arc::mojom::IntentInfoPtr intent = arc::mojom::IntentInfo::New();
  intent->action = NoteTakingHelper::kIntentAction;
  if (!clip_data_uri.is_empty())
    intent->clip_data_uri = clip_data_uri.spec();
  return intent;
}

// Returns the name of the installed app with the given `app_id`.
std::string GetAppName(Profile* profile, const std::string& app_id) {
  DCHECK(!app_id.empty());
  std::string name;
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return name;
  auto& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();

  cache.ForOneApp(app_id, [&name](const apps::AppUpdate& update) {
    if (apps_util::IsInstalled(update.Readiness()))
      name = update.Name();
  });

  if (!name.empty())
    return name;

  // TODO(crbug.com/40758396): Remove once Chrome Apps are gone or Lacros
  // launches, as note-taking Chrome Apps will not be supported in Lacros.
  const extensions::Extension* chrome_app =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          app_id);
  DCHECK(chrome_app) << "app_id must be a valid app";
  name = chrome_app->name();

  DCHECK(!name.empty()) << "app_id must be a valid app";
  return name;
}

bool IsNoteTakingIntentFilter(const apps::IntentFilterPtr& filter) {
  for (const auto& condition : filter->conditions) {
    if (condition->condition_type != apps::ConditionType::kAction)
      continue;

    for (const auto& condition_value : condition->condition_values) {
      if (condition_value->value == apps_util::kIntentActionCreateNote)
        return true;
    }
  }
  return false;
}

bool HasNoteTakingIntentFilter(
    const std::vector<apps::IntentFilterPtr>& filters) {
  for (const apps::IntentFilterPtr& filter : filters) {
    if (IsNoteTakingIntentFilter(filter))
      return true;
  }
  return false;
}

NoteTakingHelper::LaunchResult LaunchWebAppInternal(const std::string& app_id,
                                                    Profile* profile) {
  // IsInstalledWebApp must be called before trying to launch. It also ensures
  // App Service is available.
  DCHECK(IsInstalledWebApp(app_id, profile));
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  auto& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();

  bool has_note_taking_intent_filter = false;
  cache.ForOneApp(
      app_id, [&has_note_taking_intent_filter](const apps::AppUpdate& update) {
        if (HasNoteTakingIntentFilter(update.IntentFilters()))
          has_note_taking_intent_filter = true;
      });

  // Apps in 'kDefaultAllowedAppIds' might not have a note-taking intent filter.
  // They can just launch without the intent.
  if (has_note_taking_intent_filter) {
    apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
        app_id, ui::EF_NONE, apps_util::CreateCreateNoteIntent(),
        apps::LaunchSource::kFromShelf, nullptr, base::DoNothing());
  } else {
    apps::AppServiceProxyFactory::GetForProfile(profile)->Launch(
        app_id, ui::EF_NONE, apps::LaunchSource::kFromShelf);
  }

  return NoteTakingHelper::LaunchResult::WEB_APP_SUCCESS;
}

}  // namespace

const char NoteTakingHelper::kIntentAction[] =
    "org.chromium.arc.intent.action.CREATE_NOTE";
// ID of a Keep Chrome App used for dev and testing.
const char NoteTakingHelper::kDevKeepExtensionId[] =
    "ogfjaccbdfhecploibfbhighmebiffla";
const char NoteTakingHelper::kProdKeepExtensionId[] =
    "hmjkmjkepdijhoojdojkdfohbdgmmhki";
// ID of a test web app (https://yielding-large-chef.glitch.me/).
const char NoteTakingHelper::kNoteTakingWebAppIdTest[] =
    "clikhfibhokkkabhcgdhcccofienkkhj";
const char NoteTakingHelper::kPreferredLaunchResultHistogramName[] =
    "Apps.NoteTakingApp.PreferredLaunchResult";
const char NoteTakingHelper::kDefaultLaunchResultHistogramName[] =
    "Apps.NoteTakingApp.DefaultLaunchResult";

// static
void NoteTakingHelper::Initialize() {
  DCHECK(!g_helper);
  g_helper = new NoteTakingHelper();
}

// static
void NoteTakingHelper::Shutdown() {
  DCHECK(g_helper);
  delete g_helper;
  g_helper = nullptr;
}

// static
NoteTakingHelper* NoteTakingHelper::Get() {
  DCHECK(g_helper);
  return g_helper;
}

void NoteTakingHelper::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void NoteTakingHelper::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

// TODO(crbug.com/40227659): Remove this method and observe LockScreenHelper for
// app updates instead.
void NoteTakingHelper::NotifyAppUpdated(Profile* profile,
                                        const std::string& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (app_id == GetPreferredAppId(profile)) {
    for (Observer& observer : observers_) {
      observer.OnPreferredNoteTakingAppUpdated(profile);
    }
  }
}

std::vector<NoteTakingAppInfo> NoteTakingHelper::GetAvailableApps(
    Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  std::vector<NoteTakingAppInfo> infos;

  std::vector<std::string> app_ids = GetNoteTakingAppIds(profile);
  for (const auto& app_id : app_ids) {
    LockScreenAppSupport lock_screen_support =
        LockScreenApps::GetSupport(profile, app_id);
    infos.push_back(NoteTakingAppInfo{GetAppName(profile, app_id), app_id,
                                      /*preferred=*/false,
                                      lock_screen_support});
  }

  if (arc::IsArcAllowedForProfile(profile))
    infos.insert(infos.end(), android_apps_.begin(), android_apps_.end());

  // Determine which app, if any, is selected as the preferred note taking app.
  const std::string pref_app_id =
      profile->GetPrefs()->GetString(prefs::kNoteTakingAppId);
  for (auto& info : infos) {
    if (info.app_id == pref_app_id) {
      info.preferred = true;
      break;
    }
  }

  return infos;
}

std::string NoteTakingHelper::GetPreferredAppId(Profile* profile) {
  std::string app_id = profile->GetPrefs()->GetString(prefs::kNoteTakingAppId);
  if (IsInstalledApp(app_id, profile))
    return app_id;
  return std::string();
}

void NoteTakingHelper::SetPreferredApp(Profile* profile,
                                       const std::string& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  if (app_id == profile->GetPrefs()->GetString(prefs::kNoteTakingAppId))
    return;

  profile->GetPrefs()->SetString(prefs::kNoteTakingAppId, app_id);

  for (Observer& observer : observers_)
    observer.OnPreferredNoteTakingAppUpdated(profile);
}

bool NoteTakingHelper::SetPreferredAppEnabledOnLockScreen(Profile* profile,
                                                          bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  std::string app_id = profile->GetPrefs()->GetString(prefs::kNoteTakingAppId);
  if (app_id.empty())
    return false;

  LockScreenApps* lock_screen_apps =
      LockScreenAppsFactory::GetInstance()->Get(profile);
  if (!lock_screen_apps)
    return false;

  bool changed = lock_screen_apps->SetAppEnabledOnLockScreen(app_id, enabled);
  if (!changed)
    return false;

  for (Observer& observer : observers_)
    observer.OnPreferredNoteTakingAppUpdated(profile);

  return true;
}

bool NoteTakingHelper::IsAppAvailable(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  return stylus_utils::HasStylusInput() && !GetAvailableApps(profile).empty();
}

void NoteTakingHelper::LaunchAppForNewNote(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  LaunchResult result = LaunchResult::NO_APP_SPECIFIED;
  std::string app_id = profile->GetPrefs()->GetString(prefs::kNoteTakingAppId);
  if (!app_id.empty())
    result = LaunchAppInternal(profile, app_id);
  UMA_HISTOGRAM_ENUMERATION(kPreferredLaunchResultHistogramName,
                            static_cast<int>(result),
                            static_cast<int>(LaunchResult::MAX));
  if (result == LaunchResult::CHROME_SUCCESS ||
      result == LaunchResult::WEB_APP_SUCCESS ||
      result == LaunchResult::ANDROID_SUCCESS) {
    return;
  }

  // If the user hasn't chosen an app or we were unable to launch the one that
  // they've chosen, just launch the first one we see.
  result = LaunchResult::NO_APPS_AVAILABLE;
  std::vector<NoteTakingAppInfo> infos = GetAvailableApps(profile);
  if (infos.empty()) {
    LOG(WARNING) << "Unable to launch note-taking app; none available";
  } else {
    result = LaunchAppInternal(profile, infos[0].app_id);
  }
  UMA_HISTOGRAM_ENUMERATION(kDefaultLaunchResultHistogramName,
                            static_cast<int>(result),
                            static_cast<int>(LaunchResult::MAX));
}

void NoteTakingHelper::OnIntentFiltersUpdated(
    const std::optional<std::string>& package_name) {
  if (play_store_enabled_)
    UpdateAndroidApps();
}

void NoteTakingHelper::OnArcPlayStoreEnabledChanged(bool enabled) {
  play_store_enabled_ = enabled;
  if (!enabled) {
    android_apps_.clear();
    android_apps_received_ = false;
  }
  for (Observer& observer : observers_)
    observer.OnAvailableNoteTakingAppsUpdated();
}

void NoteTakingHelper::OnProfileAdded(Profile* profile) {
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    auto& cache = apps::AppServiceProxyFactory::GetForProfile(profile)
                      ->AppRegistryCache();
    if (app_registry_observations_.IsObservingSource(&cache)) {
      base::debug::DumpWithoutCrashing();
    } else {
      app_registry_observations_.AddObservation(&cache);
    }
  }

  if (!play_store_enabled_ && arc::IsArcPlayStoreEnabledForProfile(profile)) {
    play_store_enabled_ = true;
    for (Observer& observer : observers_)
      observer.OnAvailableNoteTakingAppsUpdated();
  }

  auto* bridge = arc::ArcIntentHelperBridge::GetForBrowserContext(profile);
  if (bridge) {
    if (arc_intent_helper_observations_.IsObservingSource(bridge)) {
      base::debug::DumpWithoutCrashing();
    } else {
      arc_intent_helper_observations_.AddObservation(bridge);
    }
  }
}

void NoteTakingHelper::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

NoteTakingHelper::NoteTakingHelper()
    : launch_chrome_app_callback_(
          base::BindRepeating(&apps::LaunchPlatformAppWithAction)),
      note_taking_controller_client_(
          std::make_unique<NoteTakingControllerClient>(this)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kNoteTakingAppIds);
  if (!switch_value.empty()) {
    force_allowed_app_ids_ = base::SplitString(
        switch_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }
  force_allowed_app_ids_.insert(
      force_allowed_app_ids_.end(), kDefaultAllowedAppIds,
      kDefaultAllowedAppIds + std::size(kDefaultAllowedAppIds));

  // Track profiles so we can observe their app registries.
  profile_manager_observation_.Observe(g_browser_process->profile_manager());
  play_store_enabled_ = false;
  for (Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
            profile)) {
      auto& cache = apps::AppServiceProxyFactory::GetForProfile(profile)
                        ->AppRegistryCache();
      if (app_registry_observations_.IsObservingSource(&cache)) {
        base::debug::DumpWithoutCrashing();
      } else {
        app_registry_observations_.AddObservation(&cache);
      }
    }

    // Check if the profile has already enabled Google Play Store.
    // IsArcPlayStoreEnabledForProfile() can return true only for the primary
    // profile.
    play_store_enabled_ |= arc::IsArcPlayStoreEnabledForProfile(profile);

    // ArcIntentHelperBridge will notify us about changes to the list of
    // available Android apps.
    auto* bridge = arc::ArcIntentHelperBridge::GetForBrowserContext(profile);
    if (bridge) {
      if (arc_intent_helper_observations_.IsObservingSource(bridge)) {
        base::debug::DumpWithoutCrashing();
      } else {
        arc_intent_helper_observations_.AddObservation(bridge);
      }
    }
  }

  // Watch for changes of Google Play Store enabled state.
  auto* session_manager = arc::ArcSessionManager::Get();
  session_manager->AddObserver(this);

  // If the ARC intent helper is ready, get the Android apps. Otherwise,
  // UpdateAndroidApps() will be called when ArcServiceManager calls
  // OnIntentFiltersUpdated().
  if (play_store_enabled_ && arc::ArcServiceManager::Get()
                                 ->arc_bridge_service()
                                 ->intent_helper()
                                 ->IsConnected()) {
    UpdateAndroidApps();
  }
}

NoteTakingHelper::~NoteTakingHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // ArcSessionManagerTest shuts down ARC before NoteTakingHelper.
  if (arc::ArcSessionManager::Get())
    arc::ArcSessionManager::Get()->RemoveObserver(this);
}

std::vector<std::string> NoteTakingHelper::GetNoteTakingAppIds(
    Profile* profile) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return {};

  auto& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();

  std::vector<std::string> app_ids;
  for (const auto& id : force_allowed_app_ids_) {
    cache.ForOneApp(id, [&app_ids](const apps::AppUpdate& update) {
      if (!apps_util::IsInstalled(update.Readiness()))
        return;
      if (!base::Contains(kNoteTakingAppTypes, update.AppType()))
        return;
      DCHECK(!base::Contains(app_ids, update.AppId()));
      app_ids.push_back(update.AppId());
    });
  }

  cache.ForEachApp([&app_ids](const apps::AppUpdate& update) {
    if (!apps_util::IsInstalled(update.Readiness()))
      return;
    if (base::Contains(app_ids, update.AppId()))
      return;
    if (!base::Contains(kNoteTakingAppTypes, update.AppType()))
      return;
    if (HasNoteTakingIntentFilter(update.IntentFilters())) {
      app_ids.push_back(update.AppId());
    }
  });

  return app_ids;
}

void NoteTakingHelper::UpdateAndroidApps() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* helper = ARC_GET_INSTANCE_FOR_METHOD(
      arc::ArcServiceManager::Get()->arc_bridge_service()->intent_helper(),
      RequestIntentHandlerList);
  if (!helper)
    return;
  helper->RequestIntentHandlerList(
      CreateIntentInfo(GURL()),
      base::BindOnce(&NoteTakingHelper::OnGotAndroidApps,
                     weak_ptr_factory_.GetWeakPtr()));
}

arc::mojom::ActivityNamePtr AppIdToActivityName(const std::string& id) {
  auto name = arc::mojom::ActivityName::New();

  const size_t separator = id.find('/');
  if (separator == std::string::npos) {
    name->package_name = id;
    name->activity_name = std::string();
  } else {
    name->package_name = id.substr(0, separator);
    name->activity_name = id.substr(separator + 1);
  }
  return name;
}

void NoteTakingHelper::OnGotAndroidApps(
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!play_store_enabled_)
    return;

  android_apps_.clear();
  android_apps_.reserve(handlers.size());
  for (const auto& it : handlers) {
    android_apps_.emplace_back(
        NoteTakingAppInfo{it->name, it->package_name, false,
                          LockScreenAppSupport::kNotSupported});
  }
  android_apps_received_ = true;

  for (Observer& observer : observers_)
    observer.OnAvailableNoteTakingAppsUpdated();
}

arc::mojom::OpenUrlsRequestPtr CreateArcNoteRequest(const std::string& app_id) {
  auto request = arc::mojom::OpenUrlsRequest::New();
  request->action_type = arc::mojom::ActionType::CREATE_NOTE;
  request->activity_name = AppIdToActivityName(app_id);
  return request;
}

NoteTakingHelper::LaunchResult NoteTakingHelper::LaunchAppInternal(
    Profile* profile,
    const std::string& app_id) {
  DCHECK(profile);

  // Android app.
  if (LooksLikeAndroidPackageName(app_id)) {
    // Android app.
    if (!arc::IsArcAllowedForProfile(profile)) {
      LOG(WARNING) << "Can't launch Android app " << app_id << " for profile";
      return LaunchResult::ANDROID_NOT_SUPPORTED_BY_PROFILE;
    }
    auto* helper = ARC_GET_INSTANCE_FOR_METHOD(
        arc::ArcServiceManager::Get()->arc_bridge_service()->intent_helper(),
        HandleIntent);
    if (!helper)
      return LaunchResult::ANDROID_NOT_RUNNING;

    // Only set the package name: leaving the activity name unset enables the
    // app to rename its activities.
    arc::mojom::ActivityNamePtr activity = arc::mojom::ActivityName::New();
    activity->package_name = app_id;

    auto request = CreateArcNoteRequest(app_id);
    arc::mojom::FileSystemInstance* arc_file_system =
        ARC_GET_INSTANCE_FOR_METHOD(
            arc::ArcServiceManager::Get()->arc_bridge_service()->file_system(),
            OpenUrlsWithPermissionAndWindowInfo);
    if (!arc_file_system)
      return LaunchResult::ANDROID_NOT_RUNNING;

    if (!display::HasInternalDisplay()) {
      LOG(WARNING) << "Cannot find an internal display!";
      return LaunchResult::NO_INTERNAL_DISPLAY_FOUND;
    }
    apps::WindowInfoPtr window_info = std::make_unique<apps::WindowInfo>(
        display::Display::InternalDisplayId());
    arc_file_system->OpenUrlsWithPermissionAndWindowInfo(
        std::move(request), apps::MakeArcWindowInfo(std::move(window_info)),
        base::DoNothing());

    arc::ArcMetricsService::RecordArcUserInteraction(
        profile, arc::UserInteractionType::APP_STARTED_FROM_STYLUS_TOOLS);

    return LaunchResult::ANDROID_SUCCESS;
  }

  // Web app.
  if (IsInstalledWebApp(app_id, profile))
    return LaunchWebAppInternal(app_id, profile);

  // Chrome app.
  const extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* app =
      extension_registry->enabled_extensions().GetByID(app_id);
  if (!app) {
    LOG(WARNING) << "Failed to find note-taking app " << app_id;
    return LaunchResult::CHROME_APP_MISSING;
  }
  app_runtime::ActionData action_data;
  action_data.action_type = app_runtime::ActionType::kNewNote;
  launch_chrome_app_callback_.Run(profile, app, std::move(action_data));
  return LaunchResult::CHROME_SUCCESS;
}

void NoteTakingHelper::OnAppUpdate(const apps::AppUpdate& update) {
  if (!base::Contains(kNoteTakingAppTypes, update.AppType()))
    return;
  // App was added, removed, enabled, or disabled.
  if (!update.ReadinessChanged())
    return;
  if (!base::Contains(force_allowed_app_ids_, update.AppId()) &&
      !HasNoteTakingIntentFilter(update.IntentFilters())) {
    return;
  }

  // Ok to send false positive notifications to observers.
  for (Observer& observer : observers_)
    observer.OnAvailableNoteTakingAppsUpdated();
}

void NoteTakingHelper::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_observations_.RemoveObservation(cache);
}

}  // namespace ash

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/note_taking_helper.h"

#include <ostream>
#include <utility>

#include "apps/launcher.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/mojom/intent_common.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/stylus_utils.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/note_taking_controller_client.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/pref_names.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/action_handlers_handler.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
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

// Returns whether `app_id` looks like it's probably an Android package name
// rather than a Chrome extension ID or web app ID.
bool LooksLikeAndroidPackageName(const std::string& app_id) {
  // Android package names are required to contain at least one period (see
  // validateName() in PackageParser.java), while Chrome extension IDs and web
  // app IDs contain only characters in [a-p].
  return base::Contains(app_id, '.');
}

bool IsInstalledWebApp(const std::string& app_id, Profile* profile) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return false;
  auto* cache =
      &apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();

  bool result = false;
  cache->ForOneApp(app_id, [&result](const apps::AppUpdate& update) {
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
  auto* cache =
      &apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();

  cache->ForOneApp(app_id, [&name](const apps::AppUpdate& update) {
    if (apps_util::IsInstalled(update.Readiness()))
      name = update.Name();
  });

  if (!name.empty())
    return name;

  // TODO (crbug.com/1225848): Use the name from App Service (and remove the
  // code below) once Chrome Apps are published in App Service.
  const extensions::Extension* chrome_app =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          app_id, extensions::ExtensionRegistry::ENABLED);
  DCHECK(chrome_app) << "app_id must be a valid app";
  name = chrome_app->name();

  DCHECK(!name.empty()) << "app_id must be a valid app";
  return name;
}

bool IsNoteTakingIntentFilter(const apps::mojom::IntentFilterPtr& filter) {
  for (const auto& condition : filter->conditions) {
    if (condition->condition_type != apps::mojom::ConditionType::kAction)
      continue;

    for (const auto& condition_value : condition->condition_values) {
      if (condition_value->value == apps_util::kIntentActionCreateNote)
        return true;
    }
  }
  return false;
}

bool HasNoteTakingIntentFilter(
    const std::vector<apps::mojom::IntentFilterPtr>& filters) {
  for (const apps::mojom::IntentFilterPtr& filter : filters) {
    if (IsNoteTakingIntentFilter(filter))
      return true;
  }
  return false;
}

// Whether the app's manifest indicates that the app supports note taking on the
// lock screen.
// TODO(crbug.com/1006642): Move this to a lock-screen-specific place.
bool IsLockScreenEnabled(Profile* profile, const std::string& app_id) {
  if (IsInstalledWebApp(app_id, profile)) {
    // TODO(crbug.com/1006642): Add lock screen web app support.
    return false;
  }

  // `app_id` may be for a Chrome app.
  const extensions::Extension* chrome_app =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          app_id, extensions::ExtensionRegistry::ENABLED);
  if (chrome_app) {
    if (!chrome_app->permissions_data()->HasAPIPermission(
            extensions::mojom::APIPermissionID::kLockScreen)) {
      return false;
    }
    return extensions::ActionHandlersInfo::HasLockScreenActionHandler(
        chrome_app, app_runtime::ACTION_TYPE_NEW_NOTE);
  }

  // Android apps are not currently supported on the lock screen.
  return false;
}

// Gets the set of app IDs that are allowed to be launched on the lock screen,
// if the feature is restricted using the
// `prefs::kNoteTakingAppsLockScreenAllowlist` preference. If the pref is not
// set, this method will return null (in which case the set should not be
// checked).
// Note that `prefs::kNoteTakingrAppsAllowedOnLockScreen` is currently only
// expected to be set by policy (if it's set at all).
std::unique_ptr<std::set<std::string>> GetAllowedLockScreenApps(
    PrefService* prefs) {
  const PrefService::Preference* allowed_lock_screen_apps_pref =
      prefs->FindPreference(prefs::kNoteTakingAppsLockScreenAllowlist);
  if (!allowed_lock_screen_apps_pref ||
      allowed_lock_screen_apps_pref->IsDefaultValue()) {
    return nullptr;
  }

  const base::Value* allowed_lock_screen_apps_value =
      allowed_lock_screen_apps_pref->GetValue();

  if (!allowed_lock_screen_apps_value ||
      !allowed_lock_screen_apps_value->is_list()) {
    return nullptr;
  }

  auto allowed_apps = std::make_unique<std::set<std::string>>();
  for (const base::Value& app_value :
       allowed_lock_screen_apps_value->GetListDeprecated()) {
    if (!app_value.is_string()) {
      LOG(ERROR) << "Invalid app ID value " << app_value;
      continue;
    }

    allowed_apps->insert(app_value.GetString());
  }
  return allowed_apps;
}

NoteTakingHelper::LaunchResult LaunchWebAppInternal(const std::string& app_id,
                                                    Profile* profile) {
  // IsInstalledWebApp must be called before trying to launch. It also ensures
  // App Service is available.
  DCHECK(IsInstalledWebApp(app_id, profile));
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  auto* cache =
      &apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();

  bool has_note_taking_intent_filter = false;
  cache->ForOneApp(
      app_id, [&has_note_taking_intent_filter](const apps::AppUpdate& update) {
        if (HasNoteTakingIntentFilter(update.IntentFilters()))
          has_note_taking_intent_filter = true;
      });

  // Apps in 'kDefaultAllowedAppIds' might not have a note-taking intent filter.
  // They can just launch without the intent.
  if (has_note_taking_intent_filter) {
    apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
        app_id, ui::EF_NONE, apps_util::CreateCreateNoteIntent(),
        apps::mojom::LaunchSource::kFromShelf);
  } else {
    apps::AppServiceProxyFactory::GetForProfile(profile)->Launch(
        app_id, ui::EF_NONE, apps::mojom::LaunchSource::kFromShelf);
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

std::vector<NoteTakingAppInfo> NoteTakingHelper::GetAvailableApps(
    Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  std::vector<NoteTakingAppInfo> infos;

  std::vector<std::string> app_ids = GetNoteTakingAppIds(profile);
  for (const auto& app_id : app_ids) {
    // TODO(crbug.com/1006642): Move this to a lock-screen-specific place.
    NoteTakingLockScreenSupport lock_screen_support =
        GetLockScreenSupportForAppId(profile, app_id);
    infos.push_back(NoteTakingAppInfo{GetAppName(profile, app_id), app_id,
                                      false, lock_screen_support});
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

// TODO(crbug.com/1006642): Move this to a lock-screen-specific place.
std::unique_ptr<NoteTakingAppInfo>
NoteTakingHelper::GetPreferredLockScreenAppInfo(Profile* profile) {
  std::string preferred_app_id =
      profile->GetPrefs()->GetString(prefs::kNoteTakingAppId);
  if (LooksLikeAndroidPackageName(preferred_app_id))
    return nullptr;

  if (preferred_app_id.empty())
    preferred_app_id = kProdKeepExtensionId;

  if (IsInstalledWebApp(preferred_app_id, profile)) {
    // TODO(crbug.com/1006642): Add lock screen web app support.
    return nullptr;
  }

  const extensions::Extension* chrome_app =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          preferred_app_id, extensions::ExtensionRegistry::ENABLED);
  if (!chrome_app)
    return nullptr;

  if (!IsAllowedApp(preferred_app_id) &&
      !extensions::ActionHandlersInfo::HasActionHandler(
          chrome_app, app_runtime::ACTION_TYPE_NEW_NOTE)) {
    return nullptr;
  }

  std::unique_ptr<NoteTakingAppInfo> info =
      std::make_unique<NoteTakingAppInfo>();
  info->name = chrome_app->name();
  info->app_id = preferred_app_id;
  info->preferred = true;
  info->lock_screen_support =
      GetLockScreenSupportForAppId(profile, preferred_app_id);
  return info;
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

// TODO(crbug.com/1006642): Move this to a lock-screen-specific place.
bool NoteTakingHelper::SetPreferredAppEnabledOnLockScreen(Profile* profile,
                                                          bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  if (profile != profile_with_enabled_lock_screen_apps_)
    return false;

  std::string app_id = profile->GetPrefs()->GetString(prefs::kNoteTakingAppId);
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          app_id, extensions::ExtensionRegistry::ENABLED);
  if (!app)
    return false;

  NoteTakingLockScreenSupport current_state =
      GetLockScreenSupportForAppId(profile, app_id);

  if ((enabled && current_state != NoteTakingLockScreenSupport::kSupported) ||
      (!enabled && current_state != NoteTakingLockScreenSupport::kEnabled)) {
    return false;
  }

  profile->GetPrefs()->SetBoolean(prefs::kNoteTakingAppEnabledOnLockScreen,
                                  enabled);

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
  if (infos.empty())
    LOG(WARNING) << "Unable to launch note-taking app; none available";
  else
    result = LaunchAppInternal(profile, infos[0].app_id);
  UMA_HISTOGRAM_ENUMERATION(kDefaultLaunchResultHistogramName,
                            static_cast<int>(result),
                            static_cast<int>(LaunchResult::MAX));
}

void NoteTakingHelper::OnIntentFiltersUpdated(
    const absl::optional<std::string>& package_name) {
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
  auto* registry = extensions::ExtensionRegistry::Get(profile);
  DCHECK(!extension_registry_observations_.IsObservingSource(registry));
  extension_registry_observations_.AddObservation(registry);

  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    auto* cache = &apps::AppServiceProxyFactory::GetForProfile(profile)
                       ->AppRegistryCache();
    DCHECK(cache);
    DCHECK(!app_registry_observations_.IsObservingSource(cache));
    app_registry_observations_.AddObservation(cache);
  }

  // TODO(derat): Remove this once OnArcPlayStoreEnabledChanged() is always
  // called after an ARC-enabled user logs in: http://b/36655474
  if (!play_store_enabled_ && arc::IsArcPlayStoreEnabledForProfile(profile)) {
    play_store_enabled_ = true;
    for (Observer& observer : observers_)
      observer.OnAvailableNoteTakingAppsUpdated();
  }

  auto* bridge = arc::ArcIntentHelperBridge::GetForBrowserContext(profile);
  if (bridge)
    bridge->AddObserver(this);
}

void NoteTakingHelper::SetProfileWithEnabledLockScreenApps(Profile* profile) {
  DCHECK(!profile_with_enabled_lock_screen_apps_);
  profile_with_enabled_lock_screen_apps_ = profile;

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kNoteTakingAppsLockScreenAllowlist,
      base::BindRepeating(&NoteTakingHelper::OnAllowedNoteTakingAppsChanged,
                          base::Unretained(this)));
  OnAllowedNoteTakingAppsChanged();
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
    allowed_app_ids_ = base::SplitString(
        switch_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }
  allowed_app_ids_.insert(
      allowed_app_ids_.end(), kDefaultAllowedAppIds,
      kDefaultAllowedAppIds + std::size(kDefaultAllowedAppIds));

  // Track profiles so we can observe their app registries.
  g_browser_process->profile_manager()->AddObserver(this);
  play_store_enabled_ = false;
  for (Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    // TODO(crbug.com/1225848): Remove extension_registry_observations once
    // Chrome Apps are published in App Service.
    extension_registry_observations_.AddObservation(
        extensions::ExtensionRegistry::Get(profile));

    if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
            profile)) {
      auto* cache = &apps::AppServiceProxyFactory::GetForProfile(profile)
                         ->AppRegistryCache();
      app_registry_observations_.AddObservation(cache);
    }

    // Check if the profile has already enabled Google Play Store.
    // IsArcPlayStoreEnabledForProfile() can return true only for the primary
    // profile.
    play_store_enabled_ |= arc::IsArcPlayStoreEnabledForProfile(profile);

    // ArcIntentHelperBridge will notify us about changes to the list of
    // available Android apps.
    auto* intent_helper_bridge =
        arc::ArcIntentHelperBridge::GetForBrowserContext(profile);
    if (intent_helper_bridge)
      intent_helper_bridge->AddObserver(this);
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

  g_browser_process->profile_manager()->RemoveObserver(this);

  // ArcSessionManagerTest shuts down ARC before NoteTakingHelper.
  if (arc::ArcSessionManager::Get())
    arc::ArcSessionManager::Get()->RemoveObserver(this);
  for (Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    auto* intent_helper_bridge =
        arc::ArcIntentHelperBridge::GetForBrowserContext(profile);
    if (intent_helper_bridge)
      intent_helper_bridge->RemoveObserver(this);
  }
}

bool NoteTakingHelper::IsAllowedApp(const std::string& app_id) const {
  return base::Contains(allowed_app_ids_, app_id);
}

std::vector<std::string> NoteTakingHelper::GetNoteTakingAppIds(
    Profile* profile) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::ExtensionSet& enabled_extensions =
      extension_registry->enabled_extensions();
  apps::AppRegistryCache* maybe_cache = nullptr;
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    maybe_cache = &apps::AppServiceProxyFactory::GetForProfile(profile)
                       ->AppRegistryCache();
  }

  std::vector<std::string> app_ids;
  for (const auto& id : allowed_app_ids_) {
    // TODO(crbug.com/1225848): Replace with a check for Chrome Apps in the
    // block below after Chrome Apps are published to App Service.
    if (enabled_extensions.Contains(id)) {
      app_ids.push_back(id);
      continue;
    }

    if (maybe_cache) {
      maybe_cache->ForOneApp(id, [&app_ids](const apps::AppUpdate& update) {
        if (!apps_util::IsInstalled(update.Readiness()))
          return;
        if (update.AppType() != apps::AppType::kWeb)
          return;
        DCHECK(!base::Contains(app_ids, update.AppId()));
        app_ids.push_back(update.AppId());
      });
    }
  }

  // Add any Chrome Apps that have a "note" action in their manifest
  // "action_handler" entry.
  // TODO(crbug.com/1225848): Remove when App Service publishes Chrome Apps
  // including converting action handlers to intents.
  for (const auto& extension : enabled_extensions) {
    if (base::Contains(app_ids, extension.get()->id()))
      continue;

    if (extensions::ActionHandlersInfo::HasActionHandler(
            extension.get(), app_runtime::ACTION_TYPE_NEW_NOTE)) {
      app_ids.push_back(extension.get()->id());
    }
  }

  if (maybe_cache) {
    maybe_cache->ForEachApp([&app_ids](const apps::AppUpdate& update) {
      if (!apps_util::IsInstalled(update.Readiness()))
        return;
      if (base::Contains(app_ids, update.AppId()))
        return;
      if (HasNoteTakingIntentFilter(update.IntentFilters())) {
        // Currently only web apps are expected to have this intent set.
        DCHECK(update.AppType() == apps::AppType::kWeb);
        app_ids.push_back(update.AppId());
      }
    });
  }

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
                          NoteTakingLockScreenSupport::kNotSupported});
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

    // TODO(derat): Is there some way to detect whether this fails due to the
    // package no longer being available?
    auto request = CreateArcNoteRequest(app_id);
    arc::mojom::FileSystemInstance* arc_file_system =
        ARC_GET_INSTANCE_FOR_METHOD(
            arc::ArcServiceManager::Get()->arc_bridge_service()->file_system(),
            DEPRECATED_OpenUrlsWithPermission);
    if (!arc_file_system)
      return LaunchResult::ANDROID_NOT_RUNNING;
    arc_file_system->DEPRECATED_OpenUrlsWithPermission(std::move(request),
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
  const extensions::Extension* app = extension_registry->GetExtensionById(
      app_id, extensions::ExtensionRegistry::ENABLED);
  if (!app) {
    LOG(WARNING) << "Failed to find note-taking app " << app_id;
    return LaunchResult::CHROME_APP_MISSING;
  }
  auto action_data = std::make_unique<app_runtime::ActionData>();
  action_data->action_type = app_runtime::ActionType::ACTION_TYPE_NEW_NOTE;
  launch_chrome_app_callback_.Run(profile, app, std::move(action_data));
  return LaunchResult::CHROME_SUCCESS;
}

void NoteTakingHelper::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (IsAllowedApp(extension->id()) ||
      extensions::ActionHandlersInfo::HasActionHandler(
          extension, app_runtime::ACTION_TYPE_NEW_NOTE)) {
    for (Observer& observer : observers_)
      observer.OnAvailableNoteTakingAppsUpdated();
  }
}

void NoteTakingHelper::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (IsAllowedApp(extension->id()) ||
      extensions::ActionHandlersInfo::HasActionHandler(
          extension, app_runtime::ACTION_TYPE_NEW_NOTE)) {
    for (Observer& observer : observers_)
      observer.OnAvailableNoteTakingAppsUpdated();
  }
}

void NoteTakingHelper::OnShutdown(extensions::ExtensionRegistry* registry) {
  extension_registry_observations_.RemoveObservation(registry);
}

void NoteTakingHelper::OnAppUpdate(const apps::AppUpdate& update) {
  bool is_web_app = update.AppType() == apps::AppType::kWeb;
  bool is_chrome_app =
      update.AppType() == apps::AppType::kStandaloneBrowserChromeApp;
  if (!is_web_app && !is_chrome_app)
    return;
  // App was added, removed, enabled, or disabled.
  if (!update.ReadinessChanged())
    return;

  // Ok to send false positive notifications to observers.
  for (Observer& observer : observers_)
    observer.OnAvailableNoteTakingAppsUpdated();
}

void NoteTakingHelper::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_observations_.RemoveObservation(cache);
}

// TODO(crbug.com/1006642): Move this to a lock-screen-specific place.
NoteTakingLockScreenSupport NoteTakingHelper::GetLockScreenSupportForAppId(
    Profile* profile,
    const std::string& app_id) {
  if (profile != profile_with_enabled_lock_screen_apps_)
    return NoteTakingLockScreenSupport::kNotSupported;

  if (!IsLockScreenEnabled(profile, app_id))
    return NoteTakingLockScreenSupport::kNotSupported;

  if (allowed_lock_screen_apps_state_ == AllowedAppListState::kUndetermined)
    UpdateAllowedLockScreenAppsList();

  if (allowed_lock_screen_apps_state_ ==
          AllowedAppListState::kAllowedAppsListed &&
      !base::Contains(allowed_lock_screen_apps_by_policy_, app_id)) {
    return NoteTakingLockScreenSupport::kNotAllowedByPolicy;
  }

  if (profile->GetPrefs()->GetBoolean(prefs::kNoteTakingAppEnabledOnLockScreen))
    return NoteTakingLockScreenSupport::kEnabled;

  return NoteTakingLockScreenSupport::kSupported;
}

void NoteTakingHelper::OnAllowedNoteTakingAppsChanged() {
  if (allowed_lock_screen_apps_state_ == AllowedAppListState::kUndetermined)
    return;

  std::unique_ptr<NoteTakingAppInfo> preferred_app =
      GetPreferredLockScreenAppInfo(profile_with_enabled_lock_screen_apps_);
  NoteTakingLockScreenSupport lock_screen_value_before_update =
      preferred_app ? preferred_app->lock_screen_support
                    : NoteTakingLockScreenSupport::kNotSupported;

  UpdateAllowedLockScreenAppsList();

  preferred_app =
      GetPreferredLockScreenAppInfo(profile_with_enabled_lock_screen_apps_);
  NoteTakingLockScreenSupport lock_screen_value_after_update =
      preferred_app ? preferred_app->lock_screen_support
                    : NoteTakingLockScreenSupport::kNotSupported;

  // Do not notify observers about preferred app change if its lock screen
  // support status has not actually changed.
  if (lock_screen_value_before_update != lock_screen_value_after_update) {
    for (Observer& observer : observers_) {
      observer.OnPreferredNoteTakingAppUpdated(
          profile_with_enabled_lock_screen_apps_);
    }
  }
}

void NoteTakingHelper::UpdateAllowedLockScreenAppsList() {
  std::unique_ptr<std::set<std::string>> allowed_apps =
      GetAllowedLockScreenApps(
          profile_with_enabled_lock_screen_apps_->GetPrefs());

  if (allowed_apps) {
    allowed_lock_screen_apps_state_ = AllowedAppListState::kAllowedAppsListed;
    allowed_lock_screen_apps_by_policy_.swap(*allowed_apps);
  } else {
    allowed_lock_screen_apps_state_ = AllowedAppListState::kAllAppsAllowed;
    allowed_lock_screen_apps_by_policy_.clear();
  }
}

}  // namespace ash

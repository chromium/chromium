// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lock_screen_apps/lock_screen_apps.h"

#include <memory>
#include <ostream>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ash/note_taking/note_taking_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_features.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/action_handlers_handler.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"

namespace ash {

namespace {

namespace app_runtime = ::extensions::api::app_runtime;

bool HasLockScreenIntentFilter(
    const std::vector<apps::IntentFilterPtr>& filters) {
  const auto lock_screen_intent = apps_util::CreateStartOnLockScreenIntent();
  for (const apps::IntentFilterPtr& filter : filters) {
    if (lock_screen_intent->MatchFilter(filter))
      return true;
  }
  return false;
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

// Whether the app's manifest indicates that the app supports use on the lock
// screen.
bool IsLockScreenCapable(Profile* profile, const std::string& app_id) {
  if (IsInstalledWebApp(app_id, profile)) {
    if (!base::FeatureList::IsEnabled(features::kWebLockScreenApi))
      return false;
    if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
      return false;

    auto& cache = apps::AppServiceProxyFactory::GetForProfile(profile)
                      ->AppRegistryCache();
    bool is_ready = false;
    bool has_lock_screen_intent_filter = false;
    cache.ForOneApp(app_id, [&has_lock_screen_intent_filter,
                             &is_ready](const apps::AppUpdate& update) {
      if (HasLockScreenIntentFilter(update.IntentFilters()))
        has_lock_screen_intent_filter = true;
      is_ready = update.Readiness() == apps::Readiness::kReady;
    });
    return has_lock_screen_intent_filter && is_ready;
  }

  const extensions::Extension* chrome_app =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          app_id);
  if (!chrome_app)
    return false;
  if (!chrome_app->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kLockScreen)) {
    return false;
  }
  return extensions::ActionHandlersInfo::HasLockScreenActionHandler(
      chrome_app, app_runtime::ActionType::kNewNote);
}

// Gets the set of app IDs that are allowed to be launched on the lock screen,
// if the feature is restricted using the
// `prefs::kNoteTakingAppsLockScreenAllowlist` preference. If the pref is not
// set, this method will return null (in which case the set should not be
// checked).
// Note that `prefs::kNoteTakingAppsLockScreenAllowlist` is currently only
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
       allowed_lock_screen_apps_value->GetList()) {
    if (!app_value.is_string()) {
      LOG(ERROR) << "Invalid app ID value " << app_value;
      continue;
    }

    allowed_apps->insert(app_value.GetString());
  }
  return allowed_apps;
}

}  // namespace

std::ostream& operator<<(std::ostream& out,
                         const LockScreenAppSupport& support) {
  switch (support) {
    case LockScreenAppSupport::kNotSupported:
      return out << "NotSupported";
    case LockScreenAppSupport::kNotAllowedByPolicy:
      return out << "NotAllowedByPolicy";
    case LockScreenAppSupport::kSupported:
      return out << "Supported";
    case LockScreenAppSupport::kEnabled:
      return out << "Enabled";
  }
}

// static
LockScreenAppSupport LockScreenApps::GetSupport(Profile* profile,
                                                const std::string& app_id) {
  LockScreenApps* lock_screen_apps =
      LockScreenAppsFactory::GetInstance()->Get(profile);
  if (!lock_screen_apps)
    return LockScreenAppSupport::kNotSupported;
  return lock_screen_apps->GetSupport(app_id);
}

void LockScreenApps::UpdateAllowedLockScreenAppsList() {
  std::unique_ptr<std::set<std::string>> allowed_apps =
      GetAllowedLockScreenApps(profile_->GetPrefs());

  if (allowed_apps) {
    allowed_lock_screen_apps_state_ = AllowedAppListState::kAllowedAppsListed;
    allowed_lock_screen_apps_by_policy_.swap(*allowed_apps);
  } else {
    allowed_lock_screen_apps_state_ = AllowedAppListState::kAllAppsAllowed;
    allowed_lock_screen_apps_by_policy_.clear();
  }
}

LockScreenAppSupport LockScreenApps::GetSupport(const std::string& app_id) {
  if (app_id.empty())
    return LockScreenAppSupport::kNotSupported;

  if (!IsLockScreenCapable(profile_, app_id))
    return LockScreenAppSupport::kNotSupported;

  if (allowed_lock_screen_apps_state_ == AllowedAppListState::kUndetermined)
    UpdateAllowedLockScreenAppsList();

  if (allowed_lock_screen_apps_state_ ==
          AllowedAppListState::kAllowedAppsListed &&
      !base::Contains(allowed_lock_screen_apps_by_policy_, app_id)) {
    return LockScreenAppSupport::kNotAllowedByPolicy;
  }

  // Lock screen note-taking is currently enabled/disabled for all apps at once,
  // independent of which app is preferred for note-taking. This affects the
  // toggle shown in settings UI. Currently only the preferred app can be
  // launched on the lock screen.
  // TODO(crbug.com/40099955): Consider changing this so only the preferred app
  // is reported as enabled.
  // TODO(crbug.com/40227659): Remove this dependency on note taking code by
  // migrating to a separate prefs entry.
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kNoteTakingAppEnabledOnLockScreen))
    return LockScreenAppSupport::kEnabled;

  return LockScreenAppSupport::kSupported;
}

bool LockScreenApps::SetAppEnabledOnLockScreen(const std::string& app_id,
                                               bool enabled) {
  DCHECK(!app_id.empty());

  // Currently only the preferred note-taking app is ever enabled on the lock
  // screen.
  // TODO(crbug.com/40227659): Remove this dependency on note taking code by
  // migrating to a separate prefs entry.
  DCHECK_EQ(app_id, NoteTakingHelper::Get()->GetPreferredAppId(profile_));

  LockScreenAppSupport current_state = GetSupport(app_id);

  if ((enabled && current_state != LockScreenAppSupport::kSupported) ||
      (!enabled && current_state != LockScreenAppSupport::kEnabled)) {
    return false;
  }

  // TODO(crbug.com/40227659): Migrate to a non-note-taking prefs entry.
  profile_->GetPrefs()->SetBoolean(prefs::kNoteTakingAppEnabledOnLockScreen,
                                   enabled);

  return true;
}

LockScreenApps::LockScreenApps(Profile* primary_profile)
    : profile_(primary_profile) {
  DCHECK(LockScreenAppsFactory::IsSupportedProfile(profile_));

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kNoteTakingAppsLockScreenAllowlist,
      base::BindRepeating(&LockScreenApps::OnAllowedLockScreenAppsChanged,
                          base::Unretained(this)));
  OnAllowedLockScreenAppsChanged();
}
LockScreenApps::~LockScreenApps() = default;

// Called when kNoteTakingAppsLockScreenAllowlist pref changes for `profile_`.
void LockScreenApps::OnAllowedLockScreenAppsChanged() {
  if (allowed_lock_screen_apps_state_ == AllowedAppListState::kUndetermined)
    return;

  std::string app_id = NoteTakingHelper::Get()->GetPreferredAppId(profile_);
  LockScreenAppSupport lock_screen_value_before_update = GetSupport(app_id);

  UpdateAllowedLockScreenAppsList();

  LockScreenAppSupport lock_screen_value_after_update = GetSupport(app_id);

  // Do not notify observers about preferred app change if its lock screen
  // support status has not actually changed.
  if (lock_screen_value_before_update != lock_screen_value_after_update) {
    // TODO(crbug.com/40227659): Reverse this dependency by making note taking
    // code observe this class instead.
    NoteTakingHelper::Get()->NotifyAppUpdated(profile_, app_id);
  }
}

// ---------------------------------------
// LockScreenAppsFactory implementation
// ---------------------------------------

// static
LockScreenAppsFactory* LockScreenAppsFactory::GetInstance() {
  static base::NoDestructor<LockScreenAppsFactory> instance;
  return instance.get();
}

// static
bool LockScreenAppsFactory::IsSupportedProfile(Profile* profile) {
  if (!profile)
    return false;
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return false;
  if (!ProfileHelper::IsPrimaryProfile(profile))
    return false;
  return true;
}

LockScreenApps* LockScreenAppsFactory::Get(Profile* profile) {
  return static_cast<LockScreenApps*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

LockScreenAppsFactory::LockScreenAppsFactory()
    : BrowserContextKeyedServiceFactory(
          "LockScreenApps",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

LockScreenAppsFactory::~LockScreenAppsFactory() = default;

void LockScreenAppsFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(::prefs::kNoteTakingAppsLockScreenAllowlist);
  registry->RegisterBooleanPref(::prefs::kNoteTakingAppEnabledOnLockScreen,
                                true);
}

KeyedService* LockScreenAppsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new LockScreenApps(profile);
}

content::BrowserContext* LockScreenAppsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return IsSupportedProfile(profile) ? context : nullptr;
}

}  // namespace ash

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lock_screen_apps/lock_screen_helper.h"

#include <memory>
#include <ostream>

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ash/note_taking_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/action_handlers_handler.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"

namespace ash {

namespace {

namespace app_runtime = ::extensions::api::app_runtime;

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
    // TODO(crbug.com/1006642): Add lock screen web app support.
    return false;
  }

  const extensions::Extension* chrome_app =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          app_id, extensions::ExtensionRegistry::ENABLED);
  if (!chrome_app)
    return false;
  if (!chrome_app->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kLockScreen)) {
    return false;
  }
  return extensions::ActionHandlersInfo::HasLockScreenActionHandler(
      chrome_app, app_runtime::ACTION_TYPE_NEW_NOTE);
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
       allowed_lock_screen_apps_value->GetListDeprecated()) {
    if (!app_value.is_string()) {
      LOG(ERROR) << "Invalid app ID value " << app_value;
      continue;
    }

    allowed_apps->insert(app_value.GetString());
  }
  return allowed_apps;
}

}  // namespace

LockScreenHelper& LockScreenHelper::GetInstance() {
  static base::NoDestructor<LockScreenHelper> instance;
  return *instance;
}

void LockScreenHelper::Initialize(Profile* profile) {
  DCHECK(profile);
  DCHECK(!profile_with_enabled_lock_screen_apps_);
  profile_with_enabled_lock_screen_apps_ = profile;

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kNoteTakingAppsLockScreenAllowlist,
      base::BindRepeating(&LockScreenHelper::OnAllowedLockScreenAppsChanged,
                          base::Unretained(this)));
  OnAllowedLockScreenAppsChanged();
}

// TODO(crbug.com/1332379): Remove this method. Make this class a keyed service
// so it will shutdown cleanly with the profile.
void LockScreenHelper::Shutdown() {
  profile_with_enabled_lock_screen_apps_ = nullptr;
  pref_change_registrar_.RemoveAll();
  allowed_lock_screen_apps_state_ = AllowedAppListState::kUndetermined;
  allowed_lock_screen_apps_by_policy_.clear();
}

void LockScreenHelper::UpdateAllowedLockScreenAppsList() {
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

LockScreenAppSupport LockScreenHelper::GetLockScreenSupportForApp(
    Profile* profile,
    const std::string& app_id) {
  if (profile != profile_with_enabled_lock_screen_apps_)
    return LockScreenAppSupport::kNotSupported;

  if (app_id.empty())
    return LockScreenAppSupport::kNotSupported;

  if (!IsLockScreenCapable(profile, app_id))
    return LockScreenAppSupport::kNotSupported;

  if (allowed_lock_screen_apps_state_ == AllowedAppListState::kUndetermined)
    UpdateAllowedLockScreenAppsList();

  if (allowed_lock_screen_apps_state_ ==
          AllowedAppListState::kAllowedAppsListed &&
      !base::Contains(allowed_lock_screen_apps_by_policy_, app_id)) {
    return LockScreenAppSupport::kNotAllowedByPolicy;
  }

  if (profile->GetPrefs()->GetBoolean(prefs::kNoteTakingAppEnabledOnLockScreen))
    return LockScreenAppSupport::kEnabled;

  return LockScreenAppSupport::kSupported;
}

bool LockScreenHelper::SetAppEnabledOnLockScreen(Profile* profile,
                                                 const std::string& app_id,
                                                 bool enabled) {
  DCHECK(profile);
  DCHECK(!app_id.empty());

  if (profile != profile_with_enabled_lock_screen_apps_)
    return false;

  // Currently only the preferred note-taking app is ever enabled on the lock
  // screen.
  // TODO(crbug.com/1332379): Remove this dependency on note taking code by
  // migrating to a separate prefs entry.
  DCHECK_EQ(app_id, NoteTakingHelper::Get()->GetPreferredAppId(profile));

  LockScreenAppSupport current_state =
      GetLockScreenSupportForApp(profile, app_id);

  if ((enabled && current_state != LockScreenAppSupport::kSupported) ||
      (!enabled && current_state != LockScreenAppSupport::kEnabled)) {
    return false;
  }

  // TODO(crbug.com/1332379): Migrate to a non-note-taking prefs entry.
  profile->GetPrefs()->SetBoolean(prefs::kNoteTakingAppEnabledOnLockScreen,
                                  enabled);

  return true;
}

LockScreenHelper::LockScreenHelper() = default;
LockScreenHelper::~LockScreenHelper() = default;

// Called when kNoteTakingAppsLockScreenAllowlist pref changes for
// |profile_with_enabled_lock_screen_apps_|.
void LockScreenHelper::OnAllowedLockScreenAppsChanged() {
  if (allowed_lock_screen_apps_state_ == AllowedAppListState::kUndetermined)
    return;

  std::string app_id = NoteTakingHelper::Get()->GetPreferredAppId(
      profile_with_enabled_lock_screen_apps_);
  LockScreenAppSupport lock_screen_value_before_update =
      GetLockScreenSupportForApp(profile_with_enabled_lock_screen_apps_,
                                 app_id);

  UpdateAllowedLockScreenAppsList();

  LockScreenAppSupport lock_screen_value_after_update =
      GetLockScreenSupportForApp(profile_with_enabled_lock_screen_apps_,
                                 app_id);

  // Do not notify observers about preferred app change if its lock screen
  // support status has not actually changed.
  if (lock_screen_value_before_update != lock_screen_value_after_update) {
    // TODO(crbug.com/1332379): Reverse this dependency by making note taking
    // code observe this class instead.
    NoteTakingHelper::Get()->NotifyAppUpdated(
        profile_with_enabled_lock_screen_apps_, app_id);
  }
}

}  // namespace ash

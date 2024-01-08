// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_LOCK_SCREEN_APPS_H_
#define CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_LOCK_SCREEN_APPS_H_

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace content {
class BrowserContext;
}
namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ash {

// Describes an app's level of support for use on the lock screen.
// IMPORTANT: These constants are used in settings UI, so be careful about
//     reordering/adding/removing items.
enum class LockScreenAppSupport {
  // The app does not support use on lock screen.
  kNotSupported = 0,
  // The app supports use on the lock screen, but is not allowed to run on the
  // lock screen due to policy settings.
  kNotAllowedByPolicy = 1,
  // The app supports use on the lock screen, but is not enabled as a lock
  // screen app by the user. This state implies that the user
  // can be offered to enable this app for use on the lock screen.
  kSupported = 2,
  // The app is enabled by the user to run as on the lock screen. Note that,
  // while more than one app can be in enabled state at a
  // same time, currently only the preferred note taking app will be launchable
  // from the lock screen UI.
  kEnabled = 3,
};

// For logging and debug purposes.
std::ostream& operator<<(std::ostream& out, const LockScreenAppSupport& app);

// Tracks available lock screen apps. Only exists for the primary profile.
class LockScreenApps : public KeyedService {
 public:
  // Convenience method for `GetSupport`. Returns the support status, or
  // `kNotSupported` if `profile` doesn't support lock screen apps.
  static LockScreenAppSupport GetSupport(Profile* profile,
                                         const std::string& app_id);

  LockScreenApps(const LockScreenApps&) = delete;
  LockScreenApps& operator=(const LockScreenApps&) = delete;

  // Updates the cached list of apps allowed on the lock screen. Sets
  // `allowed_lock_screen_apps_state_` and `allowed_lock_screen_apps_by_policy_`
  // to values appropriate for the current `profile_` state.
  void UpdateAllowedLockScreenAppsList();

  // Returns the state of the app's support for running on the lock screen.
  LockScreenAppSupport GetSupport(const std::string& app_id);

  // Attempts to set the given app as enabled on the lock screen. Returns
  // whether the app status changed.
  bool SetAppEnabledOnLockScreen(const std::string& app_id, bool enabled);

 private:
  explicit LockScreenApps(Profile* primary_profile);
  ~LockScreenApps() override;
  friend class LockScreenAppsFactory;

  // The state of the allowed app ID cache (used for determining the state of
  // note-taking apps allowed on the lock screen).
  enum class AllowedAppListState {
    // The allowed apps have not yet been determined.
    kUndetermined,
    // No app ID restriction exists in the profile.
    kAllAppsAllowed,
    // A list of allowed app IDs exists in the profile.
    kAllowedAppsListed
  };

  // Called when kNoteTakingAppsLockScreenAllowlist pref changes for `profile_`.
  void OnAllowedLockScreenAppsChanged();

  // The profile for which lock screen apps are enabled.
  raw_ptr<Profile> profile_ = nullptr;

  // Tracks kNoteTakingAppsLockScreenAllowlist pref for the profile for which
  // lock screen apps are enabled.
  PrefChangeRegistrar pref_change_registrar_;

  // The current AllowedAppListState for lock screen note taking in `profile_`.
  // If kAllowedAppsListed, `lock_screen_apps_allowed_by_policy_` should contain
  // the set of allowed app IDs.
  AllowedAppListState allowed_lock_screen_apps_state_ =
      AllowedAppListState::kUndetermined;

  // If `allowed_lock_screen_apps_state_` is kAllowedAppsListed, contains all
  // app IDs that are allowed to handle new-note action on the lock screen. The
  // set should only be used for apps from `profile_` and when
  // `allowed_lock_screen_apps_state_` equals kAllowedAppsListed.
  std::set<std::string> allowed_lock_screen_apps_by_policy_;
};

class LockScreenAppsFactory : public BrowserContextKeyedServiceFactory {
 public:
  LockScreenAppsFactory();
  ~LockScreenAppsFactory() override;

  static LockScreenAppsFactory* GetInstance();
  // Returns whether a profile supports lock screen apps.
  static bool IsSupportedProfile(Profile* profile);
  // Returns nullptr if profile does not support lock screen apps.
  LockScreenApps* Get(Profile* profile);

 private:
  // BrowserContextKeyedServiceFactory:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_LOCK_SCREEN_APPS_H_

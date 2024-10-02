// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOCALE_LOCALE_CHANGE_GUARD_H_
#define CHROME_BROWSER_ASH_LOCALE_LOCALE_CHANGE_GUARD_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "ash/public/cpp/locale_update_controller.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

class PrefService;
class Profile;

namespace ash {

// Performs check whether locale has been changed automatically recently
// (based on synchronized user preference). If so: shows notification that
// allows user to revert change.
class LocaleChangeGuard final : public session_manager::SessionManagerObserver,
                                public DeviceSettingsService::Observer {
 public:
  LocaleChangeGuard(Profile* profile, PrefService* local_state);

  LocaleChangeGuard(const LocaleChangeGuard&) = delete;
  LocaleChangeGuard& operator=(const LocaleChangeGuard&) = delete;

  ~LocaleChangeGuard() override;

  // Called just before changing locale.
  void PrepareChangingLocale(
      const std::string& from_locale, const std::string& to_locale);

  // Called after login.
  void OnLogin();

  void set_locale_changed_during_login(bool changed) {
    locale_changed_during_login_ = changed;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(LocaleChangeGuardTest,
                           ShowNotificationLocaleChanged);
  FRIEND_TEST_ALL_PREFIXES(LocaleChangeGuardTest,
                           ShowNotificationLocaleChangedList);

  void Check();

  void OnResult(LocaleNotificationResult result);
  void AcceptLocaleChange();
  void RevertLocaleChange();

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

  // DeviceSettingsService::Observer
  void OwnershipStatusChanged() override;

  // Whether the user has to be shown a locale update notification when the user
  // preferred locale changes from |from_locale| to |to_locale|.
  bool RequiresUserConfirmation(const std::string& from_locale,
                                const std::string& to_locale) const;

  // Returns true if we should notify user about automatic locale change.
  static bool ShouldShowLocaleChangeNotification(const std::string& from_locale,
                                                 const std::string& to_locale);

  static const char* const* GetSkipShowNotificationLanguagesForTesting();
  static size_t GetSkipShowNotificationLanguagesSizeForTesting();

  // Set if the system locale has changed on the user login. If this is true,
  // the `LocaleChangeGuard` will notify `LocaleUpdateController` that the
  // locale has changed, even if the user does not have to be shown locale
  // change notification, or if the user preferred locale has not changed.
  // Set by ProfileImple using set_locale_changed_during_login().
  bool locale_changed_during_login_ = false;

  std::string from_locale_;
  std::string to_locale_;
  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> local_state_;
  bool reverted_ = false;
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};
  base::WeakPtrFactory<LocaleChangeGuard> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOCALE_LOCALE_CHANGE_GUARD_H_

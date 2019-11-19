// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCALE_CHANGE_GUARD_H_
#define CHROME_BROWSER_CHROMEOS_LOCALE_CHANGE_GUARD_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "ash/public/cpp/locale_update_controller.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"

class Profile;

namespace chromeos {

// Performs check whether locale has been changed automatically recently
// (based on synchronized user preference).  If so: shows notification that
// allows user to revert change.
class LocaleChangeGuard : public content::NotificationObserver,
                          public session_manager::SessionManagerObserver,
                          public DeviceSettingsService::Observer,
                          public base::SupportsWeakPtr<LocaleChangeGuard> {
 public:
  explicit LocaleChangeGuard(Profile* profile);
  ~LocaleChangeGuard() override;

  // Called just before changing locale.
  void PrepareChangingLocale(
      const std::string& from_locale, const std::string& to_locale);

  // Called after login.
  void OnLogin();

 private:
  FRIEND_TEST_ALL_PREFIXES(LocaleChangeGuardTest,
                           ShowNotificationLocaleChanged);
  FRIEND_TEST_ALL_PREFIXES(LocaleChangeGuardTest,
                           ShowNotificationLocaleChangedList);

  void Check();

  void OnResult(ash::LocaleNotificationResult result);
  void AcceptLocaleChange();
  void RevertLocaleChange();

  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

  // DeviceSettingsService::Observer
  void OwnershipStatusChanged() override;

  // Returns true if we should notify user about automatic locale change.
  static bool ShouldShowLocaleChangeNotification(const std::string& from_locale,
                                                 const std::string& to_locale);

  static const char* const* GetSkipShowNotificationLanguagesForTesting();
  static size_t GetSkipShowNotificationLanguagesSizeForTesting();

  std::string from_locale_;
  std::string to_locale_;
  Profile* profile_;
  bool reverted_ = false;
  bool main_frame_loaded_ = false;
  content::NotificationRegistrar registrar_;
  ScopedObserver<session_manager::SessionManager,
                 session_manager::SessionManagerObserver>
      session_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(LocaleChangeGuard);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOCALE_CHANGE_GUARD_H_

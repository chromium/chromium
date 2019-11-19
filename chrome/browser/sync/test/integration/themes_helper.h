// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_THEMES_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_THEMES_HELPER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;
class ThemeService;

namespace themes_helper {

// Gets the unique ID of the custom theme with the given index.
std::string GetCustomTheme(int index) WARN_UNUSED_RESULT;

// Gets the ID of |profile|'s theme.
std::string GetThemeID(Profile* profile) WARN_UNUSED_RESULT;

// Returns true iff |profile| is using a custom theme.
bool UsingCustomTheme(Profile* profile) WARN_UNUSED_RESULT;

// Returns true iff |profile| is using the default theme.
bool UsingDefaultTheme(Profile* profile) WARN_UNUSED_RESULT;

// Returns true iff |profile| is using the system theme.
bool UsingSystemTheme(Profile* profile) WARN_UNUSED_RESULT;

// Returns true iff a theme with the given ID is pending install in
// |profile|.
bool ThemeIsPendingInstall(
    Profile* profile, const std::string& id) WARN_UNUSED_RESULT;

// Sets |profile| to use the custom theme with the given index.
void UseCustomTheme(Profile* profile, int index);

// Sets |profile| to use the default theme.
void UseDefaultTheme(Profile* profile);

// Sets |profile| to use the system theme.
void UseSystemTheme(Profile* profile);

}  // namespace themes_helper

// Waits until |profile| is using the system theme.
// Returns false in case of timeout.

// Waits until |profile| is using the default theme.
// Returns false in case of timeout.

// Helper to wait until a given condition is met, checking every time the
// current theme changes.
//
// The |exit_condition_| closure may be invoked zero or more times.
class ThemeConditionChecker : public StatusChangeChecker,
                              public content::NotificationObserver {
 public:
  ThemeConditionChecker(Profile* profile,
                        const std::string& debug_message_,
                        base::Callback<bool(ThemeService*)> exit_condition);
  ~ThemeConditionChecker() override;

  // Implementation of StatusChangeChecker.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // Implementation of content::NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  Profile* profile_;
  const std::string debug_message_;
  base::Callback<bool(ThemeService*)> exit_condition_;

  content::NotificationRegistrar registrar_;
};

// Waits until |theme| is pending for install on |profile|.
// Returns false in case of timeout.

// Helper to wait until the specified theme is pending for install on the
// specified profile.
//
// The themes sync integration tests don't actually install any custom themes,
// but they do occasionally check that the ThemeService attempts to install
// synced themes.
class ThemePendingInstallChecker : public StatusChangeChecker,
                                   public content::NotificationObserver {
 public:
  ThemePendingInstallChecker(Profile* profile, const std::string& theme);
  ~ThemePendingInstallChecker() override;

  // Implementation of StatusChangeChecker.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // Implementation of content::NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  Profile* profile_;
  const std::string& theme_;

  content::NotificationRegistrar registrar_;
};

class SystemThemeChecker : public ThemeConditionChecker {
 public:
  explicit SystemThemeChecker(Profile* profile);
};

class DefaultThemeChecker : public ThemeConditionChecker {
 public:
  explicit DefaultThemeChecker(Profile* profile);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_THEMES_HELPER_H_

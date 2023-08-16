// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_THEMES_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_THEMES_HELPER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/themes/theme_service_observer.h"

class Profile;
class ThemeService;

namespace themes_helper {

bool IsSystemThemeDistinctFromDefaultTheme(Profile* profile);

// Gets the unique ID of the custom theme with the given index.
[[nodiscard]] std::string GetCustomTheme(int index);

// Gets the ID of |profile|'s theme.
[[nodiscard]] std::string GetThemeID(Profile* profile);

// Returns true iff |profile| is using a custom theme.
[[nodiscard]] bool UsingCustomTheme(Profile* profile);

// Returns true iff |profile| is using the default theme.
[[nodiscard]] bool UsingDefaultTheme(Profile* profile);

// Returns true iff |profile| is using the system theme.
[[nodiscard]] bool UsingSystemTheme(Profile* profile);

// Returns true iff a theme with the given ID is pending install in
// |profile|.
[[nodiscard]] bool ThemeIsPendingInstall(Profile* profile,
                                         const std::string& id);

// Sets |profile| to use the custom theme with the given index.
void UseCustomTheme(Profile* profile, int index);

// Sets |profile| to use the default theme.
void UseDefaultTheme(Profile* profile);

// Sets |profile| to use the system theme.
void UseSystemTheme(Profile* profile);

}  // namespace themes_helper

// Helper to wait until a given condition is met, checking every time the
// current theme changes.
//
// The |exit_condition_| closure may be invoked zero or more times.
class ThemeConditionChecker : public StatusChangeChecker,
                              public ThemeServiceObserver {
 public:
  ThemeConditionChecker(
      Profile* profile,
      const std::string& debug_message_,
      const base::RepeatingCallback<bool(ThemeService*)>& exit_condition);
  ~ThemeConditionChecker() override;

  // Implementation of StatusChangeChecker.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // Implementation of ThemeServiceObserver.
  void OnThemeChanged() override;

 private:
  const raw_ptr<Profile> profile_;
  const std::string debug_message_;
  base::RepeatingCallback<bool(ThemeService*)> exit_condition_;
};

// Waits until |theme| is pending for install on |profile|.
// Returns false in case of timeout.

// Helper to wait until the specified theme is pending for install on the
// specified profile.
//
// The themes sync integration tests don't actually install any custom themes,
// but they do occasionally check that the ThemeService attempts to install
// synced themes.
class ThemePendingInstallChecker : public StatusChangeChecker {
 public:
  ThemePendingInstallChecker(Profile* profile, const std::string& theme);
  ~ThemePendingInstallChecker() override;

  // Implementation of StatusChangeChecker.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const raw_ptr<Profile> profile_;
  const raw_ref<const std::string> theme_;

  base::WeakPtrFactory<ThemePendingInstallChecker> weak_ptr_factory_{this};
};

// Waits until |profile| is using the system theme.
// Returns false in case of timeout.
class SystemThemeChecker : public ThemeConditionChecker {
 public:
  explicit SystemThemeChecker(Profile* profile);
};

// Waits until |profile| is using the default theme.
// Returns false in case of timeout.
class DefaultThemeChecker : public ThemeConditionChecker {
 public:
  explicit DefaultThemeChecker(Profile* profile);
};

// Waits until |profile| is using a custom theme.
// Returns false in case of timeout.
class CustomThemeChecker : public ThemeConditionChecker {
 public:
  explicit CustomThemeChecker(Profile* profile);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_THEMES_HELPER_H_

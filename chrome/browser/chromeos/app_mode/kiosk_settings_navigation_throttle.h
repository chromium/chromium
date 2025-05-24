// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_SETTINGS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_SETTINGS_NAVIGATION_THROTTLE_H_

#include <memory>
#include <string>
#include <vector>

#include "content/public/browser/navigation_throttle.h"

namespace chromeos {

// Throttle that is applied on WebContents which are opening settings pages
// opened in kiosk mode. It restricts the navigations inside of these
// WebContents to the list of allowed urls.
class KioskSettingsNavigationThrottle : public content::NavigationThrottle {
 public:
  struct SettingsPage {
    const char* url = nullptr;
    bool allow_subpages = false;
  };

  // Returns the default list of settings pages allowed to open in Kiosk mode.
  // Can be overridden in tests with `SetSettingPagesForTesting`.
  static const std::vector<SettingsPage>& DefaultSettingsPages();

  // Whether this page is a settings page that is allowed to be open in kiosk
  // mode.
  static bool IsSettingsPage(const std::string& url);

  // Replaces the list of allowed settings plages with the provided one.
  static void SetSettingPagesForTesting(const std::vector<SettingsPage>* pages);

  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);
  explicit KioskSettingsNavigationThrottle(
      content::NavigationThrottleRegistry& registry);

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult WillStartOrRedirectRequest();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_SETTINGS_NAVIGATION_THROTTLE_H_

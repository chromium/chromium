// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_NAVIGATION_CAPTURING_SETTINGS_CROS_IMPL_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_NAVIGATION_CAPTURING_SETTINGS_CROS_IMPL_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/navigation_capturing_settings.h"

class Profile;

namespace web_app {

// This is created in the `NavigationCapturingSettings::Create` method, called
// in `NavigationCapturingProcess`.
//
// ChromeOS uses a different source of truth for storing the preferred
// application to capture navigations given a url, as it was implemented first
// and it has to also deal with ARC++ apps. This specialization queries that
// instead of the WebAppRegistrar.
class NavigationCapturingSettingsCrosImpl : public NavigationCapturingSettings {
 public:
  explicit NavigationCapturingSettingsCrosImpl(Profile& profile);
  ~NavigationCapturingSettingsCrosImpl() override;
  NavigationCapturingSettingsCrosImpl(
      const NavigationCapturingSettingsCrosImpl&) = delete;
  NavigationCapturingSettingsCrosImpl& operator=(
      const NavigationCapturingSettingsCrosImpl&) = delete;

  std::optional<webapps::AppId> GetCapturingWebAppForUrl(
      const GURL& url) override;

  // Allows ChromeOS to sometimes disable auxiliary browser context handling in
  // the NavigationCapturingProcess, as ChromeOsWebAppExperiments has some
  // custom handling for auxiliary `about:blank` urls that then redirect.
  // Otherwise falls back to default implementation.
  bool ShouldAuxiliaryContextsKeepSameContainer(
      const std::optional<webapps::AppId>& source_browser_app_id,
      const GURL& url) override;

 private:
  raw_ref<Profile> profile_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_NAVIGATION_CAPTURING_SETTINGS_CROS_IMPL_H_

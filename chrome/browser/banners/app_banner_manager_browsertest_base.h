// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_BROWSERTEST_BASE_H_

#include <string>

#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#endif

// Common base class for browser tests exercising AppBannerManager. Contains
// methods for generating test URLs that trigger app banners.
class AppBannerManagerBrowserTestBase : public PlatformBrowserTest {
 public:
  AppBannerManagerBrowserTestBase();

  AppBannerManagerBrowserTestBase(const AppBannerManagerBrowserTestBase&) =
      delete;
  AppBannerManagerBrowserTestBase& operator=(
      const AppBannerManagerBrowserTestBase&) = delete;

  ~AppBannerManagerBrowserTestBase() override;
  void SetUpOnMainThread() override;

 protected:
  // Executes JavaScript in |script| in the active WebContents of |browser|,
  // possibly with a user gesture depending on |with_gesture|.
  static void ExecuteScript(content::WebContents* web_contents,
                            const std::string& script,
                            bool with_gesture);

  content::WebContents* web_contents();

  Profile* profile();

  // Returns a test server URL to a page with generates a banner.
  GURL GetBannerURL();

  // Returns a test server URL with "action" = |value| set in the query string.
  GURL GetBannerURLWithAction(const std::string& action);

  // Returns a test server URL with |manifest_url| injected as the manifest tag.
  GURL GetBannerURLWithManifest(const std::string& manifest_url);

  // Returns a test server URL with |manifest_url| injected as the manifest tag
  // and |key| = |value| in the query string.
  GURL GetBannerURLWithManifestAndQuery(const std::string& manifest_url,
                                        const std::string& key,
                                        const std::string& value);

#if !BUILDFLAG(IS_ANDROID)
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
#endif
};

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_BROWSERTEST_BASE_H_

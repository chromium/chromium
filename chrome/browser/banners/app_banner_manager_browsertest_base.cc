// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_browsertest_base.h"

#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#endif

AppBannerManagerBrowserTestBase::AppBannerManagerBrowserTestBase() = default;

AppBannerManagerBrowserTestBase::~AppBannerManagerBrowserTestBase() = default;

void AppBannerManagerBrowserTestBase::SetUpOnMainThread() {
  ASSERT_TRUE(embedded_test_server()->Start());

  PlatformBrowserTest::SetUpOnMainThread();

#if !BUILDFLAG(IS_ANDROID)
  web_app::test::WaitUntilReady(
      web_app::WebAppProvider::GetForTest(browser()->profile()));
#endif
}

GURL AppBannerManagerBrowserTestBase::GetBannerURL() {
  return embedded_test_server()->GetURL("/banners/manifest_test_page.html");
}

// static
void AppBannerManagerBrowserTestBase::ExecuteScript(
    content::WebContents* web_contents,
    const std::string& script,
    bool with_gesture) {
  if (with_gesture)
    EXPECT_TRUE(content::ExecJs(web_contents, script));
  else
    EXPECT_TRUE(content::ExecJs(web_contents, script,
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

content::WebContents* AppBannerManagerBrowserTestBase::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

Profile* AppBannerManagerBrowserTestBase::profile() {
  return chrome_test_utils::GetProfile(this);
}

GURL AppBannerManagerBrowserTestBase::GetBannerURLWithAction(
    const std::string& action) {
  GURL url = GetBannerURL();
  return net::AppendQueryParameter(url, "action", action);
}

GURL AppBannerManagerBrowserTestBase::GetBannerURLWithManifest(
    const std::string& manifest_url) {
  GURL url = GetBannerURL();
  return net::AppendQueryParameter(
      url, "manifest", embedded_test_server()->GetURL(manifest_url).spec());
}

GURL AppBannerManagerBrowserTestBase::GetBannerURLWithManifestAndQuery(
    const std::string& manifest_url,
    const std::string& key,
    const std::string& value) {
  GURL url = GetBannerURLWithManifest(manifest_url);
  return net::AppendQueryParameter(url, key, value);
}

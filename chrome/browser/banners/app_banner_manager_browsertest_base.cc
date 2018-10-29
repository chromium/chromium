// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

AppBannerManagerBrowserTestBase::AppBannerManagerBrowserTestBase() = default;

AppBannerManagerBrowserTestBase::~AppBannerManagerBrowserTestBase() = default;

void AppBannerManagerBrowserTestBase::SetUpOnMainThread() {
  ASSERT_TRUE(embedded_test_server()->Start());

  InProcessBrowserTest::SetUpOnMainThread();
}

GURL AppBannerManagerBrowserTestBase::GetBannerURL() {
  return embedded_test_server()->GetURL("/banners/manifest_test_page.html");
}

// static
void AppBannerManagerBrowserTestBase::ExecuteScript(Browser* browser,
                                                    const std::string& script,
                                                    bool with_gesture) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (with_gesture)
    EXPECT_TRUE(content::ExecuteScript(web_contents, script));
  else
    EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(web_contents, script));
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

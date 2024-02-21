// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_tab_helper.h"

#include "base/path_service.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_content_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace policy {

using testing::_;
using testing::Return;

namespace {

const DlpContentRestrictionSet kEmptyRestrictionSet;
const DlpContentRestrictionSet kScreenshotRestrictionSet(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kBlock);

}  // namespace

class DlpContentTabHelperBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  DlpContentTabHelperBrowserTest()
      : scoped_dlp_content_observer_(&mock_dlp_content_observer_),
        ignore_dlp_rules_manager_(
            DlpContentTabHelper::IgnoreDlpRulesManagerForTesting()) {}

 protected:
  void SetUp() override { extensions::PlatformAppBrowserTest::SetUp(); }

  void TearDown() override { extensions::PlatformAppBrowserTest::TearDown(); }

  MockDlpContentObserver mock_dlp_content_observer_;
  ScopedDlpContentObserverForTesting scoped_dlp_content_observer_;
  DlpContentTabHelper::ScopedIgnoreDlpRulesManager ignore_dlp_rules_manager_;
};

IN_PROC_BROWSER_TEST_F(DlpContentTabHelperBrowserTest, PlatformApp) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ExtensionTestMessageListener launched_listener("Launched");

  // Install Platform App
  content::CreateAndLoadWebContentsObserver app_loaded_observer;
  const extensions::Extension* extension = InstallPlatformApp("dlp_test");
  ASSERT_TRUE(extension);

  // Restrict screenshot for Platform App
  GURL kUrl = GURL("chrome-extension://" + extension->id() + "/index.html");
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      GURL(), kEmptyRestrictionSet);
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      kUrl, kScreenshotRestrictionSet);
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, kScreenshotRestrictionSet))
      .Times(1);

  // Launch Platform App
  LaunchPlatformApp(extension);
  app_loaded_observer.Wait();
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  EXPECT_TRUE(web_contents);
  EXPECT_NE(nullptr,
            policy::DlpContentTabHelper::FromWebContents(web_contents));
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, kEmptyRestrictionSet))
      .Times(1);
  EXPECT_CALL(mock_dlp_content_observer_, OnWebContentsDestroyed(_)).Times(2);
}

class DlpContentTabHelperBFCacheBrowserTest : public InProcessBrowserTest {
 public:
  DlpContentTabHelperBFCacheBrowserTest()
      : scoped_dlp_content_observer_(&mock_dlp_content_observer_),
        ignore_dlp_rules_manager_(
            DlpContentTabHelper::IgnoreDlpRulesManagerForTesting()) {
    bfcache_feature_list_.InitWithFeaturesAndParameters(
        content::GetBasicBackForwardCacheFeatureForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  MockDlpContentObserver mock_dlp_content_observer_;

 private:
  ScopedDlpContentObserverForTesting scoped_dlp_content_observer_;
  DlpContentTabHelper::ScopedIgnoreDlpRulesManager ignore_dlp_rules_manager_;
  base::test::ScopedFeatureList bfcache_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DlpContentTabHelperBFCacheBrowserTest,
                       BackForwardPreservesRestrictionSet) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Restrict screenshot for restricted.com, and not for unrestricted.com
  GURL kUrlRestricted =
      embedded_test_server()->GetURL("restricted.com", "/title1.html");
  GURL kUrlUnrestricted =
      embedded_test_server()->GetURL("unrestricted.com", "/title1.html");
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      GURL(), kEmptyRestrictionSet);
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      kUrlRestricted, kScreenshotRestrictionSet);
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      kUrlUnrestricted, kEmptyRestrictionSet);

  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(web_contents);

  // 1) navigate to restricted.com
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, kScreenshotRestrictionSet))
      .Times(1);
  EXPECT_TRUE(content::NavigateToURL(web_contents, kUrlRestricted));
  content::RenderFrameHost* const rfh_a = web_contents->GetPrimaryMainFrame();
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) navigate to unrestricted.com
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, kEmptyRestrictionSet))
      .Times(1);
  EXPECT_TRUE(content::NavigateToURL(web_contents, kUrlUnrestricted));
  content::RenderFrameHost* const rfh_b = web_contents->GetPrimaryMainFrame();
  content::RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_FALSE(delete_observer_rfh_a.deleted())
      << "Expected rfh_a to be in BackForwardCache";

  // 3) Navigate back to restricted.com
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, kScreenshotRestrictionSet))
      .Times(1);
  web_contents->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_FALSE(delete_observer_rfh_b.deleted())
      << "Expected rfh_b to be in BackForwardCache";

  // 4) Navigate forward to unrestricted.com
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, kEmptyRestrictionSet))
      .Times(1);
  web_contents->GetController().GoForward();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  EXPECT_TRUE(policy::DlpContentTabHelper::FromWebContents(web_contents));
  EXPECT_CALL(mock_dlp_content_observer_, OnWebContentsDestroyed(_)).Times(1);
}

}  // namespace policy

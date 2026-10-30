// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_tab_helper.h"

#include "base/no_destructor.h"
#include "base/path_service.h"
#include "chrome/browser/apps/app_service/chrome_app_deprecation/chrome_app_deprecation.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_content_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
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

const DlpContentRestrictionSet& GetEmptyRestrictionSet() {
  static const base::NoDestructor<DlpContentRestrictionSet> val;
  return *val;
}
const DlpContentRestrictionSet& GetScreenshotRestrictionSet() {
  static const base::NoDestructor<DlpContentRestrictionSet> val(
      DlpContentRestriction::kScreenshot, DlpRulesManager::Level::kBlock);
  return *val;
}

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
  apps::chrome_app_deprecation::ScopedAddAppToAllowlistForTesting allowlist(
      extension->id());

  // Restrict screenshot for Platform App
  GURL kUrl = GURL("chrome-extension://" + extension->id() + "/index.html");
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      GURL(), GetEmptyRestrictionSet());
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      kUrl, GetScreenshotRestrictionSet());
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, GetScreenshotRestrictionSet()))
      .Times(1);

  // Launch Platform App
  LaunchPlatformApp(extension);
  app_loaded_observer.Wait();
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  EXPECT_TRUE(web_contents);
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, GetEmptyRestrictionSet()))
      .Times(1);
  EXPECT_CALL(mock_dlp_content_observer_, OnWebContentsDestroyed(_)).Times(2);
}

class DlpContentTabHelperWebViewBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  DlpContentTabHelperWebViewBrowserTest() = default;

  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  DlpContentTabHelper::ScopedIgnoreDlpRulesManager ignore_dlp_rules_manager_{
      DlpContentTabHelper::IgnoreDlpRulesManagerForTesting()};
};

IN_PROC_BROWSER_TEST_F(DlpContentTabHelperWebViewBrowserTest,
                       WebViewCaptureVisibleRegionBlocked) {
  const GURL confidential_url = embedded_test_server()->GetURL("/title1.html");
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      confidential_url, GetScreenshotRestrictionSet());

  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("dlp_test", "Launched");
  ASSERT_TRUE(extension);

  content::WebContents* app_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(app_web_contents);

  const std::string script = content::JsReplace(
      R"(
        new Promise(resolve => {
          const wv = document.createElement('webview');
          wv.style.width = '200px';
          wv.style.height = '200px';
          wv.addEventListener('loadstop', () => {
            wv.captureVisibleRegion({format: 'png'}, (dataUrl) => {
              if (chrome.runtime.lastError) {
                resolve('ERROR: ' + chrome.runtime.lastError.message);
              } else if (dataUrl && dataUrl.startsWith('data:image/png')) {
                resolve('SUCCESS');
              } else {
                resolve('ERROR: Unknown error');
              }
            });
          });
          wv.src = $1;
          document.body.appendChild(wv);
        });
      )",
      confidential_url);

  EXPECT_EQ("ERROR: Failed to capture webview: screenshot has been disabled",
            content::EvalJs(app_web_contents, script));
}

IN_PROC_BROWSER_TEST_F(DlpContentTabHelperWebViewBrowserTest,
                       UnattachedWebViewNavigatesToConfidentialUrl) {
  const GURL opener_url = embedded_test_server()->GetURL("/title2.html");
  const GURL confidential_url = embedded_test_server()->GetURL("/title1.html");
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      confidential_url, GetScreenshotRestrictionSet());

  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("dlp_test", "Launched");
  ASSERT_TRUE(extension);

  content::WebContents* app_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(app_web_contents);

  content::CreateAndLoadWebContentsObserver opener_loaded_observer;
  EXPECT_EQ("LOADED", content::EvalJs(app_web_contents, content::JsReplace(
                                                            R"(
            new Promise(resolve => {
              const openerWv = document.createElement('webview');
              openerWv.style.width = '200px';
              openerWv.style.height = '200px';
              window.newWindowCreatedPromise = new Promise(resolveNewWindow => {
                openerWv.addEventListener('newwindow', e => {
                  e.preventDefault();
                  const newWv = document.createElement('webview');
                  newWv.style.width = '200px';
                  newWv.style.height = '200px';
                  e.window.attach(newWv);
                  window.unattachedWebview = newWv;
                  resolveNewWindow('NEWWINDOW_READY');
                }, {once: true});
              });
              openerWv.addEventListener('loadstop', () => resolve('LOADED'),
                                        {once: true});
              openerWv.src = $1;
              document.body.appendChild(openerWv);
            })
          )",
                                                            opener_url)));
  content::WebContents* opener_guest_web_contents =
      opener_loaded_observer.Wait();
  ASSERT_TRUE(opener_guest_web_contents);

  // Open `confidential_url` in a new guest window with 'noopener'. In the
  // embedder's `newwindow` handler, `e.window.attach(newWv)` is called on a
  // detached <webview> element (`newWv` is not in the DOM yet), so the new
  // guest WebContents loads `confidential_url` while remaining unattached
  // (`GetNativeView() == nullptr`).
  content::CreateAndLoadWebContentsObserver unattached_guest_loaded_observer;
  content::ExecuteScriptAsync(
      opener_guest_web_contents,
      content::JsReplace("window.open($1, '_blank', 'noopener');",
                         confidential_url));

  content::WebContents* guest_web_contents =
      unattached_guest_loaded_observer.Wait();
  ASSERT_TRUE(guest_web_contents);
  ASSERT_NE(opener_guest_web_contents, guest_web_contents);
  EXPECT_EQ(
      "NEWWINDOW_READY",
      content::EvalJs(app_web_contents, "window.newWindowCreatedPromise"));

  // While unattached, the guest WebContents has no native view, is tracked as
  // confidential, and does not crash DlpContentManagerAsh or apply on-screen
  // restrictions prematurely.
  EXPECT_EQ(nullptr, guest_web_contents->GetNativeView());
  EXPECT_EQ(GetScreenshotRestrictionSet(),
            DlpContentManager::Get()->GetConfidentialRestrictions(
                guest_web_contents));
  EXPECT_TRUE(
      DlpContentManager::Get()->IsScreenshotApiRestricted(guest_web_contents));
  EXPECT_EQ(GetEmptyRestrictionSet(),
            DlpContentManagerAsh::Get()->GetOnScreenPresentRestrictions());

  // Append `window.unattachedWebview` to the DOM to complete attachment, wait
  // for its queued `loadstop` event, and verify DLP blocks screenshots and
  // applies on-screen restrictions once attached.
  EXPECT_EQ("ERROR: Failed to capture webview: screenshot has been disabled",
            content::EvalJs(app_web_contents, R"(
              new Promise(resolve => {
                const wv = window.unattachedWebview;
                wv.addEventListener('loadstop', () => {
                  wv.captureVisibleRegion({format: 'png'}, () => {
                    if (chrome.runtime.lastError) {
                      resolve('ERROR: ' + chrome.runtime.lastError.message);
                    } else {
                      resolve('SUCCESS');
                    }
                  });
                }, {once: true});
                document.body.appendChild(wv);
              })
            )"));
  EXPECT_NE(nullptr, guest_web_contents->GetNativeView());
  EXPECT_EQ(GetScreenshotRestrictionSet(),
            DlpContentManagerAsh::Get()->GetOnScreenPresentRestrictions());
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
      GURL(), GetEmptyRestrictionSet());
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      kUrlRestricted, GetScreenshotRestrictionSet());
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      kUrlUnrestricted, GetEmptyRestrictionSet());

  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(web_contents);

  // 1) navigate to restricted.com
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, GetScreenshotRestrictionSet()))
      .Times(1);
  EXPECT_TRUE(content::NavigateToURL(web_contents, kUrlRestricted));
  content::RenderFrameHost* const rfh_a = web_contents->GetPrimaryMainFrame();
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) navigate to unrestricted.com
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, GetEmptyRestrictionSet()))
      .Times(1);
  EXPECT_TRUE(content::NavigateToURL(web_contents, kUrlUnrestricted));
  content::RenderFrameHost* const rfh_b = web_contents->GetPrimaryMainFrame();
  content::RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_FALSE(delete_observer_rfh_a.deleted())
      << "Expected rfh_a to be in BackForwardCache";

  // 3) Navigate back to restricted.com
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, GetScreenshotRestrictionSet()))
      .Times(1);
  web_contents->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_FALSE(delete_observer_rfh_b.deleted())
      << "Expected rfh_b to be in BackForwardCache";

  // 4) Navigate forward to unrestricted.com
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, GetEmptyRestrictionSet()))
      .Times(1);
  web_contents->GetController().GoForward();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  EXPECT_TRUE(policy::DlpContentTabHelper::FromWebContents(web_contents));
  EXPECT_CALL(mock_dlp_content_observer_, OnWebContentsDestroyed(_)).Times(1);
}

}  // namespace policy

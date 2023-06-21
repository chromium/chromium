// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/ml_promotion_browsertest_base.h"
#include "base/test/test_future.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#endif

namespace webapps {

MLPromotionBrowserTestBase::MLPromotionBrowserTestBase()
    : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
#if !BUILDFLAG(IS_ANDROID)
  os_hooks_suppress_.emplace();
#endif  // !BUILDFLAG(IS_ANDROID)
}

MLPromotionBrowserTestBase::~MLPromotionBrowserTestBase() = default;

void MLPromotionBrowserTestBase::SetUp() {
// TODO(b/287255120) : Build functionalities for Android.
#if !BUILDFLAG(IS_ANDROID)
  https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  webapps::TestAppBannerManagerDesktop::SetUp();
#endif  // !BUIDLFLAG(IS_ANDROID)

  PlatformBrowserTest::SetUp();
}

void MLPromotionBrowserTestBase::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();
  // By default, all SSL cert checks are valid. Can be overridden in tests.
  cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(https_server()->Start());

// TODO(b/287255120) : Build functionalities for Android.
#if !BUILDFLAG(IS_ANDROID)
  web_app::test::WaitUntilReady(
      web_app::WebAppProvider::GetForTest(browser()->profile()));
#endif  // !BUILDFLAG(IS_ANDROID)
}

net::EmbeddedTestServer* MLPromotionBrowserTestBase::https_server() {
  return &https_server_;
}

bool MLPromotionBrowserTestBase::InstallAppForCurrentWebContents(
    bool install_locally) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(b/287255120) : Build functionalities for Android.
  return false;
#else
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForTest(browser()->profile());
  base::test::TestFuture<const web_app::AppId&, InstallResultCode>
      install_future;

  provider->scheduler().FetchManifestAndInstall(
      WebappInstallSource::OMNIBOX_INSTALL_ICON, web_contents()->GetWeakPtr(),
      /*bypass_service_worker_check=*/true,
      base::BindOnce(web_app::test::TestAcceptDialogCallback),
      install_future.GetCallback(), /*use_fallback=*/false);

  bool success = install_future.Wait();
  if (!success) {
    return success;
  }

  const web_app::AppId& app_id = install_future.Get<web_app::AppId>();
  provider->sync_bridge_unsafe().SetAppIsLocallyInstalledForTesting(
      app_id, /*is_locally_installed=*/install_locally);
  return success;
#endif  // BUILDFLAG(IS_ANDROID)
}

bool MLPromotionBrowserTestBase::NavigateAndAwaitInstallabilityCheck(
    const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(b/287255120) : Build functionalities for Android.
  return false;
#else
  auto* manager = TestAppBannerManagerDesktop::FromWebContents(web_contents());
  web_app::NavigateToURLAndWait(browser(), url);
  return manager->WaitForInstallableCheck();
#endif  // BUILDFLAG(IS_ANDROID)
}

segmentation_platform::MockSegmentationPlatformService*
MLPromotionBrowserTestBase::GetMockSegmentation(
    content::WebContents* custom_web_contents) {
  if (!custom_web_contents) {
    custom_web_contents = web_contents();
  }
#if BUILDFLAG(IS_ANDROID)
  // TODO(b/287255120) : Build functionalities for Android.
  return nullptr;
#else
  return TestAppBannerManagerDesktop::FromWebContents(custom_web_contents)
      ->GetMockSegmentationPlatformService();
#endif  // BUILDFLAG(IS_ANDROID)
}

content::WebContents* MLPromotionBrowserTestBase::web_contents() {
#if BUILDFLAG(IS_ANDROID)
  // TODO(b/287255120) : Build functionalities for Android.
  return nullptr;
#else
  return browser()->tab_strip_model()->GetActiveWebContents();
#endif  // BUILDFLAG(IS_ANDROID)
}

Profile* MLPromotionBrowserTestBase::profile() {
#if BUILDFLAG(IS_ANDROID)
  // TODO(b/287255120) : Build functionalities for Android.
  return nullptr;
#else
  return browser()->profile();
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace webapps

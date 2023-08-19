// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_TEST_APP_BANNER_MANAGER_DESKTOP_H_
#define CHROME_BROWSER_BANNERS_TEST_APP_BANNER_MANAGER_DESKTOP_H_

#include "base/values.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

namespace segmentation_platform {
class MockSegmentationPlatformService;
}  // namespace segmentation_platform

namespace webapps {

// Provides the ability to await the results of the installability check that
// happens for every page load.
class TestAppBannerManagerDesktop : public AppBannerManagerDesktop {
 public:
  explicit TestAppBannerManagerDesktop(content::WebContents* web_contents);

  TestAppBannerManagerDesktop(const TestAppBannerManagerDesktop&) = delete;
  TestAppBannerManagerDesktop& operator=(const TestAppBannerManagerDesktop&) =
      delete;

  ~TestAppBannerManagerDesktop() override;

  // Ensure this test class will be instantiated in place of
  // AppBannerManagerDesktop. Must be called before AppBannerManagerDesktop is
  // first instantiated.
  static void SetUp();

  static TestAppBannerManagerDesktop* FromWebContents(
      content::WebContents* contents);

  // Blocks until the existing installability check has been cleared.
  void WaitForInstallableCheckTearDown();

  // Returns whether both the installable and promotable check passed.
  bool WaitForInstallableCheck();

  // Configures a callback to be invoked when the app banner flow finishes.
  void PrepareDone(base::OnceClosure on_done);

  // Returns the internal state of the AppBannerManager.
  AppBannerManager::State state();

  // Block until the current app has been installed.
  void AwaitAppInstall();

  segmentation_platform::MockSegmentationPlatformService*
  GetMockSegmentationPlatformService();

  // AppBannerManager:
  void OnDidGetManifest(const InstallableData& result) override;
  void OnDidPerformInstallableWebAppCheck(
      const InstallableData& result) override;
  void ResetCurrentPageData() override;
  segmentation_platform::SegmentationPlatformService*
  GetSegmentationPlatformService() override;

  // AppBannerManagerDesktop:
  TestAppBannerManagerDesktop* AsTestAppBannerManagerDesktopForTesting()
      override;

 protected:
  // AppBannerManager:
  void OnInstall(blink::mojom::DisplayMode display) override;
  void DidFinishCreatingWebApp(const web_app::AppId& app_id,
                               webapps::InstallResultCode code) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void UpdateState(AppBannerManager::State state) override;
  void RecheckInstallabilityForLoadedPage() override;

 private:
  void SetInstallable(bool installable);
  void SetPromotable(bool promotable);
  void OnFinished();

  absl::optional<bool> installable_;
  base::Value::List debug_log_;
  base::OnceClosure tear_down_quit_closure_;
  base::OnceClosure installable_quit_closure_;
  base::OnceClosure promotable_quit_closure_;
  base::OnceClosure on_done_;
  base::OnceClosure on_install_;
  std::unique_ptr<segmentation_platform::MockSegmentationPlatformService>
      segmentation_platform_service_;
};

}  // namespace webapps

#endif  // CHROME_BROWSER_BANNERS_TEST_APP_BANNER_MANAGER_DESKTOP_H_

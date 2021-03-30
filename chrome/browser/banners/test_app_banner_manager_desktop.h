// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_TEST_APP_BANNER_MANAGER_DESKTOP_H_
#define CHROME_BROWSER_BANNERS_TEST_APP_BANNER_MANAGER_DESKTOP_H_

#include "chrome/browser/banners/app_banner_manager_desktop.h"

#include "base/macros.h"
#include "base/optional.h"

namespace content {
class WebContents;
}

namespace webapps {

// Provides the ability to await the results of the installability check that
// happens for every page load.
class TestAppBannerManagerDesktop : public AppBannerManagerDesktop {
 public:
  explicit TestAppBannerManagerDesktop(content::WebContents* web_contents);
  ~TestAppBannerManagerDesktop() override;

  // Ensure this test class will be instantiated in place of
  // AppBannerManagerDesktop. Must be called before AppBannerManagerDesktop is
  // first instantiated.
  static void SetUp();

  static TestAppBannerManagerDesktop* FromWebContents(
      content::WebContents* contents);

  // Blocks until the existing installability check has been cleared.
  void WaitForInstallableCheckTearDown();

  // Returns whether the installable check passed.
  bool WaitForInstallableCheck();

  // Configures a callback to be invoked when the app banner flow finishes.
  void PrepareDone(base::OnceClosure on_done);

  // Returns the internal state of the AppBannerManager.
  AppBannerManager::State state();

  // Block until the current app has been installed.
  void AwaitAppInstall();

  // AppBannerManager:
  void OnDidGetManifest(const InstallableData& result) override;
  void OnDidPerformInstallableWebAppCheck(
      const InstallableData& result) override;
  void ResetCurrentPageData() override;

  // AppBannerManagerDesktop:
  TestAppBannerManagerDesktop* AsTestAppBannerManagerDesktopForTesting()
      override;

 protected:
  // AppBannerManager:
  void OnInstall(blink::mojom::DisplayMode display) override;
  void DidFinishCreatingWebApp(const web_app::AppId& app_id,
                               web_app::InstallResultCode code) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void UpdateState(AppBannerManager::State state) override;

 private:
  void SetInstallable(bool installable);
  void OnFinished();

  base::Optional<bool> installable_;
  base::OnceClosure tear_down_quit_closure_;
  base::OnceClosure installable_quit_closure_;
  base::OnceClosure on_done_;
  base::OnceClosure on_install_;

  DISALLOW_COPY_AND_ASSIGN(TestAppBannerManagerDesktop);
};

}  // namespace webapps

#endif  // CHROME_BROWSER_BANNERS_TEST_APP_BANNER_MANAGER_DESKTOP_H_

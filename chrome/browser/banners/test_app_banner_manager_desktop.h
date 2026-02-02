// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_TEST_APP_BANNER_MANAGER_DESKTOP_H_
#define CHROME_BROWSER_BANNERS_TEST_APP_BANNER_MANAGER_DESKTOP_H_

#include <optional>

#include "base/values.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"

namespace content {
class WebContents;
}

namespace webapps {

// Provides the ability to await the results of the installability check that
// happens for every page load.
class TestAppBannerManagerDesktop : public AppBannerManagerDesktop,
                                    private AppBannerManager::Observer {
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
  void SetBannerPromptReplyCallback(base::OnceClosure on_banner_prompt_reply);

  // Configures a callback to be invoked when the app banner flow finishes.
  void SetCompleteCallback(base::OnceClosure on_complete);

  // Returns the internal state of the AppBannerManager.
  AppBannerManager::State state();

  // Block until the current app has been installed.
  void AwaitAppInstall();

  // AppBannerManagerDesktop:
  void OnWebAppInstallableCheckedNoErrors(
      const ManifestId& manifest_id) override;
  void ResetCurrentPageData() override;
  TestAppBannerManagerDesktop* AsTestAppBannerManagerDesktopForTesting()
      override;

  const base::ListValue& debug_log() const { return debug_log_; }

 protected:
  // AppBannerManager:
  // TODO(http://crbug.com/322342499): When AppBannerManager is devirtualized,
  // listen to WebContentsObserver::DidFinishLoad directly instead.
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 private:
  void RunInstallableQuitClosureIfNeeded();

  // AppBannerManager::Observer:
  void OnInstallableWebAppStatusUpdated(
      InstallableWebAppCheckResult,
      const std::optional<WebAppBannerData>&) override {}
  void WillFetchManifest() override;
  void OnInstall() override;
  void OnBannerPromptReply() override;
  void OnComplete() override;

  bool installable_check_in_progress_ = true;
  base::ListValue debug_log_;
  base::OnceClosure tear_down_quit_closure_;
  base::OnceClosure installable_quit_closure_;
  base::OnceClosure on_banner_prompt_reply_;
  base::OnceClosure on_complete_;
  base::OnceClosure on_install_;
};

}  // namespace webapps

#endif  // CHROME_BROWSER_BANNERS_TEST_APP_BANNER_MANAGER_DESKTOP_H_

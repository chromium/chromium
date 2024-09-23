// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"

#include "base/test/gmock_callback_support.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list_mock_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

class TabDesktopMediaListIwaTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  TabDesktopMediaListIwaTest() = default;

  TabDesktopMediaListIwaTest(const TabDesktopMediaListIwaTest&) = delete;
  TabDesktopMediaListIwaTest& operator=(const TabDesktopMediaListIwaTest&) =
      delete;

  void CreateDefaultList() {
    CHECK(!list_);

    list_ = std::make_unique<TabDesktopMediaList>(
        browser()->tab_strip_model()->GetWebContentsAt(0),
        base::BindRepeating(
            [](content::WebContents* contents) { return true; }),
        /*include_chrome_app_windows=*/false);

    // Set update period to reduce the time it takes to run tests.
    // >0 to avoid unit test failure.
    list_->SetUpdatePeriod(base::Milliseconds(1));

    // TabDesktopMediaList needs an observer that implements
    // DesktopMediaListObserver to update
    list_->StartUpdating(&observer_);
  }

  void InstallAndOpenIsolatedWebApp() {
    auto app = web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder())
                   .BuildBundle();
    app->TrustSigningKey();
    web_app::IsolatedWebAppUrlInfo url_info = app->Install(profile()).value();

    OpenApp(url_info.app_id());
  }

  const TabDesktopMediaList& list() const { return *list_; }

 private:
  DesktopMediaListMockObserver observer_;
  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server_;
  std::unique_ptr<TabDesktopMediaList> list_;
};

IN_PROC_BROWSER_TEST_F(TabDesktopMediaListIwaTest,
                       IwaNotVisibleInTabMediaList) {
  CreateDefaultList();
  int initial_list_size = list().GetSourceCount();

  InstallAndOpenIsolatedWebApp();

  EXPECT_EQ(initial_list_size, list().GetSourceCount());
}

IN_PROC_BROWSER_TEST_F(TabDesktopMediaListIwaTest,
                       NewTabVisibleInTabMediaList) {
  CreateDefaultList();
  int initial_list_size = list().GetSourceCount();

  chrome::NewTab(browser());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(initial_list_size + 1, list().GetSourceCount());
}

class TabDesktopMediaListWithIwaIncludedTest
    : public TabDesktopMediaListIwaTest {
  void SetUp() override {
    base::test::ScopedFeatureList scoped_feature_list_;
    scoped_feature_list_.InitWithFeatureState(
        features::kRemovalOfIWAsFromTabCapture, false);

    TabDesktopMediaListIwaTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(TabDesktopMediaListWithIwaIncludedTest,
                       IwaVisibleInTabMediaListWhenFeatureIsDisabled) {
  CreateDefaultList();
  int initial_list_size = list().GetSourceCount();

  InstallAndOpenIsolatedWebApp();

  EXPECT_EQ(initial_list_size + 1, list().GetSourceCount());
}
}  // namespace

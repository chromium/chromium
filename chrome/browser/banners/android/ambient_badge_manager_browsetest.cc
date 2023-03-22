// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/android/ambient_badge_manager.h"
#include "components/webapps/browser/android/app_banner_manager_android.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "content/public/test/browser_test.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace webapps {

class TestAmbientBadgeManager : public AmbientBadgeManager {
 public:
  explicit TestAmbientBadgeManager(
      content::WebContents* web_contents,
      base::WeakPtr<AppBannerManagerAndroid> app_banner_manager)
      : AmbientBadgeManager(web_contents, app_banner_manager) {}

  TestAmbientBadgeManager(const TestAmbientBadgeManager&) = delete;
  TestAmbientBadgeManager& operator=(const TestAmbientBadgeManager&) = delete;

  ~TestAmbientBadgeManager() override = default;

  void WaitForState(State target, base::OnceClosure on_done) {
    target_state_ = target;
    on_done_ = std::move(on_done);
  }

 protected:
  void UpdateState(State state) override {
    AmbientBadgeManager::UpdateState(state);
    if (state == target_state_ && on_done_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_done_));
    }
  }

 private:
  State target_state_;
  base::OnceClosure on_done_;
};

class TestAppBannerManager : public AppBannerManagerAndroid {
 public:
  explicit TestAppBannerManager(content::WebContents* web_contents)
      : AppBannerManagerAndroid(web_contents) {}

  TestAppBannerManager(const TestAppBannerManager&) = delete;
  TestAppBannerManager& operator=(const TestAppBannerManager&) = delete;

  ~TestAppBannerManager() override = default;

  void WaitForAmbientBadgeState(AmbientBadgeManager::State target,
                                base::OnceClosure on_done) {
    target_badge_state_ = target;
    on_badge_done_ = std::move(on_done);
  }

  TestAmbientBadgeManager* GetBadgeManagerForTest() {
    return ambient_badge_test_.get();
  }

 protected:
  void MaybeShowAmbientBadge() override {
    ambient_badge_test_ = std::make_unique<TestAmbientBadgeManager>(
        web_contents(), GetAndroidWeakPtr());

    ambient_badge_test_->WaitForState(target_badge_state_,
                                      std::move(on_badge_done_));
    ambient_badge_test_->MaybeShow(
        validated_url_, GetAppName(),
        CreateAddToHomescreenParams(InstallableMetrics::GetInstallSource(
            web_contents(), InstallTrigger::AMBIENT_BADGE)),
        base::BindOnce(&AppBannerManagerAndroid::ShowBannerFromBadge,
                       AppBannerManagerAndroid::GetAndroidWeakPtr()));
  }

  void OnDidPerformWorkerCheckForAmbientBadge(
      const InstallableData& data) override {
    if (ambient_badge_test_) {
      ambient_badge_test_->OnWorkerCheckResult(data);
    }
  }

 private:
  std::unique_ptr<TestAmbientBadgeManager> ambient_badge_test_;
  AmbientBadgeManager::State target_badge_state_;
  base::OnceClosure on_badge_done_;
};

class AmbientBadgeManagerBrowserTest : public AndroidBrowserTest {
 public:
  AmbientBadgeManagerBrowserTest() = default;

  AmbientBadgeManagerBrowserTest(const AmbientBadgeManagerBrowserTest&) =
      delete;
  AmbientBadgeManagerBrowserTest& operator=(
      const AmbientBadgeManagerBrowserTest&) = delete;

  ~AmbientBadgeManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    site_engagement::SiteEngagementScore::SetParamValuesForTesting();

    AndroidBrowserTest::SetUpOnMainThread();
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  Profile* profile() {
    return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  }

  void RunTest(const GURL& url, AmbientBadgeManager::State expected_state) {
    site_engagement::SiteEngagementService* service =
        site_engagement::SiteEngagementService::Get(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    service->ResetBaseScoreForURL(url, 10);

    auto app_banner_manager =
        std::make_unique<TestAppBannerManager>(web_contents());
    base::RunLoop waiter;

    app_banner_manager->WaitForAmbientBadgeState(expected_state,
                                                 waiter.QuitClosure());
    ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

    waiter.Run();
  }
};

IN_PROC_BROWSER_TEST_F(AmbientBadgeManagerBrowserTest, ShowAmbientBadge) {
  RunTest(embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
          AmbientBadgeManager::State::SHOWING);
}

IN_PROC_BROWSER_TEST_F(AmbientBadgeManagerBrowserTest, NoServiceWorker) {
  RunTest(embedded_test_server()->GetURL(
              "/banners/manifest_no_service_worker.html"),
          AmbientBadgeManager::State::PENDING_WORKER);
}

}  // namespace webapps

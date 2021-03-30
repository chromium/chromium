// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/android/optimization_guide_tab_url_provider_android.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {
namespace android {
namespace {

using ::testing::ElementsAre;

// FakeTabModel that can be used for testing Android tab behavior.
class FakeTabModel : public TabModel {
 public:
  explicit FakeTabModel(
      Profile* profile,
      const std::vector<content::WebContents*>& web_contents_list)
      : TabModel(profile, chrome::android::ActivityType::kCustomTab),
        web_contents_list_(web_contents_list) {}

  int GetTabCount() const override {
    return static_cast<int>(web_contents_list_.size());
  }
  int GetActiveIndex() const override { return 0; }
  content::WebContents* GetWebContentsAt(int index) const override {
    if (index < static_cast<int>(web_contents_list_.size()))
      return web_contents_list_[index];
    return nullptr;
  }

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const override {
    return nullptr;
  }
  void CreateTab(TabAndroid* parent,
                 content::WebContents* web_contents) override {}
  void HandlePopupNavigation(TabAndroid* parent,
                             NavigateParams* params) override {}
  content::WebContents* CreateNewTabForDevTools(const GURL& url) override {
    return nullptr;
  }
  bool IsSessionRestoreInProgress() const override { return false; }
  bool IsActiveModel() const override { return false; }
  TabAndroid* GetTabAt(int index) const override { return nullptr; }
  void SetActiveIndex(int index) override {}
  void CloseTabAt(int index) override {}
  void AddObserver(TabModelObserver* observer) override {}
  void RemoveObserver(TabModelObserver* observer) override {}

 private:
  std::vector<content::WebContents*> web_contents_list_;
};

class OptimizationGuideTabUrlProviderAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  OptimizationGuideTabUrlProviderAndroidTest() = default;
  ~OptimizationGuideTabUrlProviderAndroidTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tab_url_provider_ =
        std::make_unique<OptimizationGuideTabUrlProviderAndroid>(profile());
  }

  void TearDown() override {
    tab_url_provider_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  OptimizationGuideTabUrlProviderAndroid* tab_url_provider() const {
    return tab_url_provider_.get();
  }

 private:
  std::unique_ptr<OptimizationGuideTabUrlProviderAndroid> tab_url_provider_;
};

TEST_F(OptimizationGuideTabUrlProviderAndroidTest,
       GetUrlsOfActiveTabsNoOpenTabs) {
  std::vector<GURL> urls =
      tab_url_provider()->GetUrlsOfActiveTabs(base::TimeDelta::FromDays(90));
  EXPECT_TRUE(urls.empty());
}

TEST_F(OptimizationGuideTabUrlProviderAndroidTest,
       GetUrlsOfActiveTabsFiltersOutTabs) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents.get());
  web_contents_tester->SetLastCommittedURL(GURL("https://example.com/a"));
  std::unique_ptr<content::WebContents> web_contents2 =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  content::WebContentsTester* web_contents_tester2 =
      content::WebContentsTester::For(web_contents2.get());
  web_contents_tester2->SetLastCommittedURL(GURL("https://example.com/b"));
  web_contents_tester2->SetLastActiveTime(base::TimeTicks::Now() -
                                          base::TimeDelta::FromDays(2));
  std::unique_ptr<content::WebContents> stale_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  content::WebContentsTester* stale_web_contents_tester =
      content::WebContentsTester::For(stale_web_contents.get());
  stale_web_contents_tester->SetLastActiveTime(base::TimeTicks::Now() -
                                               base::TimeDelta::FromDays(100));
  stale_web_contents_tester->SetLastCommittedURL(GURL("https://stale.com"));
  FakeTabModel tab_model(profile(), {web_contents.get(), web_contents2.get(),
                                     stale_web_contents.get()});
  TabModelList::AddTabModel(&tab_model);

  std::unique_ptr<content::WebContents> otr_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  content::WebContentsTester* otr_web_contents_tester =
      content::WebContentsTester::For(otr_web_contents.get());
  otr_web_contents_tester->SetLastCommittedURL(GURL("https://incognito.com"));
  FakeTabModel otr_tab_model(profile()->GetPrimaryOTRProfile(),
                             {otr_web_contents.get()});
  TabModelList::AddTabModel(&otr_tab_model);

  std::vector<GURL> urls =
      tab_url_provider()->GetUrlsOfActiveTabs(base::TimeDelta::FromDays(90));
  EXPECT_THAT(urls, ElementsAre(GURL("https://example.com/a"),
                                GURL("https://example.com/b")));
}

}  // namespace
}  // namespace android
}  // namespace optimization_guide

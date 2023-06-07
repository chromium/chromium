// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/android/optimization_guide_tab_url_provider_android.h"

#include "base/time/time.h"
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

using FakeTab = std::pair<GURL, absl::optional<base::TimeTicks>>;

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

}  // namespace

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

  std::vector<GURL> GetSortedURLsForTabs(
      const std::vector<std::vector<FakeTab>>& fake_tabs) {
    std::vector<OptimizationGuideTabUrlProviderAndroid::TabRepresentation> tabs;
    for (size_t tab_model_idx = 0; tab_model_idx < fake_tabs.size();
         tab_model_idx++) {
      for (size_t tab_idx = 0; tab_idx < fake_tabs[tab_model_idx].size();
           tab_idx++) {
        OptimizationGuideTabUrlProviderAndroid::TabRepresentation tab;
        tab.tab_model_index = tab_model_idx;
        tab.tab_index = tab_idx;
        std::pair<GURL, absl::optional<base::TimeTicks>> fake_tab =
            fake_tabs[tab_model_idx][tab_idx];
        tab.url = fake_tab.first;
        tab.last_active_time = fake_tab.second;
        tabs.push_back(tab);
      }
    }
    tab_url_provider_->SortTabs(&tabs);

    std::vector<GURL> sorted_urls;
    sorted_urls.reserve(tabs.size());
    for (const auto& tab : tabs) {
      sorted_urls.push_back(tab.url);
    }
    return sorted_urls;
  }

 private:
  std::unique_ptr<OptimizationGuideTabUrlProviderAndroid> tab_url_provider_;
};

TEST_F(OptimizationGuideTabUrlProviderAndroidTest,
       GetUrlsOfActiveTabsNoOpenTabs) {
  std::vector<GURL> urls =
      tab_url_provider()->GetUrlsOfActiveTabs(base::Days(90));
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
  web_contents_tester->SetLastActiveTime(base::TimeTicks::Now() -
                                         base::Days(3));
  std::unique_ptr<content::WebContents> web_contents2 =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  content::WebContentsTester* web_contents_tester2 =
      content::WebContentsTester::For(web_contents2.get());
  web_contents_tester2->SetLastCommittedURL(GURL("https://example.com/b"));
  web_contents_tester2->SetLastActiveTime(base::TimeTicks::Now() -
                                          base::Days(2));
  std::unique_ptr<content::WebContents> stale_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  content::WebContentsTester* stale_web_contents_tester =
      content::WebContentsTester::For(stale_web_contents.get());
  stale_web_contents_tester->SetLastActiveTime(base::TimeTicks::Now() -
                                               base::Days(100));
  stale_web_contents_tester->SetLastCommittedURL(GURL("https://stale.com"));
  FakeTabModel tab_model(profile(), {web_contents.get(), web_contents2.get(),
                                     stale_web_contents.get(), nullptr});
  TabModelList::AddTabModel(&tab_model);

  std::unique_ptr<content::WebContents> otr_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  content::WebContentsTester* otr_web_contents_tester =
      content::WebContentsTester::For(otr_web_contents.get());
  otr_web_contents_tester->SetLastCommittedURL(GURL("https://incognito.com"));
  FakeTabModel otr_tab_model(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      {otr_web_contents.get()});
  TabModelList::AddTabModel(&otr_tab_model);

  std::vector<GURL> urls =
      tab_url_provider()->GetUrlsOfActiveTabs(base::Days(90));
  EXPECT_THAT(urls, ElementsAre(GURL("https://example.com/b"),
                                GURL("https://example.com/a")));
}

TEST_F(OptimizationGuideTabUrlProviderAndroidTest, SortsTabsCorrectly) {
  std::vector<std::vector<FakeTab>> fake_tabs;
  fake_tabs.push_back({
      std::make_pair(GURL("https://example.com/third"),
                     base::TimeTicks::Now() - base::Days(3)),
      std::make_pair(GURL("https://example.com/second"),
                     base::TimeTicks::Now() - base::Days(2)),
      std::make_pair(GURL("https://example.com/0-2"), absl::nullopt),
  });
  fake_tabs.push_back({
      std::make_pair(GURL("https://example.com/first"),
                     base::TimeTicks::Now() - base::Days(1)),
      std::make_pair(GURL("https://example.com/1-1"), absl::nullopt),
  });

  EXPECT_THAT(GetSortedURLsForTabs(fake_tabs),
              ElementsAre(GURL("https://example.com/first"),
                          GURL("https://example.com/second"),
                          GURL("https://example.com/third"),
                          GURL("https://example.com/0-2"),
                          GURL("https://example.com/1-1")));
}

}  // namespace android
}  // namespace optimization_guide

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/auxiliary_search/auxiliary_search_provider.h"

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_android.h"
#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {
constexpr size_t kMaxDonatedTabs = 2;
constexpr char kMaxDonatedTabsValue[] = "2";
}  // namespace

class AuxiliarySearchProviderBrowserTest : public AndroidBrowserTest {
 public:
  AuxiliarySearchProviderBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        chrome::android::kAuxiliarySearchDonation,
        {{"auxiliary_search_max_donation_tab", kMaxDonatedTabsValue}});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(content::NavigateToURL(
        web_contents(),
        embedded_test_server()->GetURL("/android/google.html")));
    auxiliary_search_provider_ = std::make_unique<AuxiliarySearchProvider>(
        BookmarkModelFactory::GetForBrowserContext(profile()));
    PersistedTabDataAndroid::OnDeferredStartup();
    content::RunAllTasksUntilIdle();
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  AuxiliarySearchProvider* provider() {
    return auxiliary_search_provider_.get();
  }

  Profile* profile() {
    auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
    return Profile::FromBrowserContext(web_contents->GetBrowserContext());
  }

  std::vector<raw_ptr<TabAndroid, VectorExperimental>> CreateOneTab(
      bool is_sensitive) {
    std::vector<raw_ptr<TabAndroid, VectorExperimental>> tab_vec;
    TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
    tab_vec.push_back(tab_android);
    std::unique_ptr<SensitivityPersistedTabDataAndroid> sptda =
        std::make_unique<SensitivityPersistedTabDataAndroid>(tab_android);
    sptda->set_is_sensitive(is_sensitive);
    tab_android->SetUserData(SensitivityPersistedTabDataAndroid::UserDataKey(),
                             std::move(sptda));
    return tab_vec;
  }

 private:
  std::unique_ptr<AuxiliarySearchProvider> auxiliary_search_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AuxiliarySearchProviderBrowserTest, QuerySensitiveTab) {
  base::RunLoop run_loop;
  std::vector<raw_ptr<TabAndroid, VectorExperimental>> tab_vec =
      CreateOneTab(true);

  provider()->GetNonSensitiveTabsInternal(
      tab_vec,
      base::BindOnce(
          [](base::OnceClosure done,
             std::vector<base::WeakPtr<TabAndroid>> non_sensitive_tabs) {
            EXPECT_EQ(0u, non_sensitive_tabs.size());
            std::move(done).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AuxiliarySearchProviderBrowserTest,
                       QueryNonSensitiveTab) {
  base::RunLoop run_loop;
  std::vector<raw_ptr<TabAndroid, VectorExperimental>> tab_vec =
      CreateOneTab(false);

  TabModel* tab_model = TabModelList::GetTabModelForWebContents(web_contents());
  TabAndroid* second_tab = TabAndroid::FromWebContents(web_contents());
  std::unique_ptr<content::WebContents> contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  content::WebContents* second_web_contents = contents.release();
  tab_model->CreateTab(second_tab, second_web_contents, /*select=*/true);
  std::unique_ptr<SensitivityPersistedTabDataAndroid> sptda2 =
      std::make_unique<SensitivityPersistedTabDataAndroid>(second_tab);
  sptda2->set_is_sensitive(false);
  tab_vec.push_back(second_tab);

  provider()->GetNonSensitiveTabsInternal(
      tab_vec,
      base::BindOnce(
          [](base::OnceClosure done,
             std::vector<base::WeakPtr<TabAndroid>> non_sensitive_tabs) {
            EXPECT_EQ(2u, non_sensitive_tabs.size());
            std::move(done).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AuxiliarySearchProviderBrowserTest,
                       QueryNonSensitiveTab_flagTest) {
  base::RunLoop run_loop;
  std::vector<raw_ptr<TabAndroid, VectorExperimental>> tab_vec =
      CreateOneTab(false);

  TabModel* tab_model = TabModelList::GetTabModelForWebContents(web_contents());
  TabAndroid* second_tab = TabAndroid::FromWebContents(web_contents());
  std::unique_ptr<content::WebContents> contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  content::WebContents* second_web_contents = contents.release();
  tab_model->CreateTab(second_tab, second_web_contents, /*select=*/true);
  std::unique_ptr<SensitivityPersistedTabDataAndroid> sptda2 =
      std::make_unique<SensitivityPersistedTabDataAndroid>(second_tab);
  sptda2->set_is_sensitive(false);
  tab_vec.push_back(second_tab);

  TabAndroid* third_tab = TabAndroid::FromWebContents(web_contents());
  contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  content::WebContents* third_web_contents = contents.release();
  tab_model->CreateTab(third_tab, third_web_contents, /*select=*/true);
  std::unique_ptr<SensitivityPersistedTabDataAndroid> sptda3 =
      std::make_unique<SensitivityPersistedTabDataAndroid>(third_tab);
  sptda3->set_is_sensitive(false);
  tab_vec.push_back(third_tab);

  provider()->GetNonSensitiveTabsInternal(
      tab_vec,
      base::BindOnce(
          [](base::OnceClosure done,
             std::vector<base::WeakPtr<TabAndroid>> non_sensitive_tabs) {
            // Only 2 should be here since the flag is set to 2.
            EXPECT_EQ(kMaxDonatedTabs, non_sensitive_tabs.size());
            std::move(done).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AuxiliarySearchProviderBrowserTest, QueryEmptyTabList) {
  base::RunLoop run_loop;

  provider()->GetNonSensitiveTabsInternal(
      std::vector<raw_ptr<TabAndroid, VectorExperimental>>(),
      base::BindOnce(
          [](base::OnceClosure done,
             std::vector<base::WeakPtr<TabAndroid>> non_sensitive_tabs) {
            EXPECT_EQ(0u, non_sensitive_tabs.size());
            std::move(done).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AuxiliarySearchProviderBrowserTest, NativeTabTest) {
  base::RunLoop run_loop;
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(url::kAboutBlankURL)));
  std::vector<raw_ptr<TabAndroid, VectorExperimental>> tab_vec =
      CreateOneTab(false);

  provider()->GetNonSensitiveTabsInternal(
      tab_vec,
      base::BindOnce(
          [](base::OnceClosure done,
             std::vector<base::WeakPtr<TabAndroid>> non_sensitive_tabs) {
            EXPECT_EQ(0u, non_sensitive_tabs.size());
            std::move(done).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AuxiliarySearchProviderBrowserTest, FilterTabsTest) {
  struct {
    const GURL url;
    bool should_be_filtered;
  } test_cases[]{
      {GURL(url::kAboutBlankURL), true},
      {GURL("chrome://version"), true},
      {GURL("chrome-native://newtab"), true},
      {embedded_test_server()->GetURL("/android/google.html"), false},
  };

  for (const auto& test_case : test_cases) {
    ASSERT_TRUE(content::NavigateToURL(web_contents(), test_case.url));
    std::vector<raw_ptr<TabAndroid, VectorExperimental>> tab_vec =
        CreateOneTab(false);

    AuxiliarySearchProvider::FilterTabsByScheme(tab_vec);
    EXPECT_EQ(test_case.should_be_filtered ? 0u : 1u, tab_vec.size());
  }
}

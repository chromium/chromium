// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/auxiliary_search/auxiliary_search_provider.h"

#include "base/run_loop.h"
#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

class AuxiliarySearchProviderBrowserTest : public AndroidBrowserTest {
 public:
  AuxiliarySearchProviderBrowserTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    auxiliary_search_provider_ =
        std::make_unique<AuxiliarySearchProvider>(profile());
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

 private:
  std::unique_ptr<AuxiliarySearchProvider> auxiliary_search_provider_;
};

IN_PROC_BROWSER_TEST_F(AuxiliarySearchProviderBrowserTest, QuerySensitiveTab) {
  base::RunLoop run_loop;
  std::unique_ptr<std::vector<TabAndroid*>> tab_vec =
      std::make_unique<std::vector<TabAndroid*>>();
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
  tab_vec->push_back(tab_android);
  std::unique_ptr<SensitivityPersistedTabDataAndroid> sptda =
      std::make_unique<SensitivityPersistedTabDataAndroid>(tab_android);
  sptda->set_is_sensitive(true);
  tab_android->SetUserData(SensitivityPersistedTabDataAndroid::UserDataKey(),
                           std::move(sptda));

  provider()->GetNonSensitiveTabsInternal(
      std::move(tab_vec),
      base::BindOnce(
          [](base::OnceClosure done,
             std::unique_ptr<std::vector<TabAndroid*>> non_Sensitive_tab) {
            EXPECT_EQ(0u, non_Sensitive_tab->size());
            std::move(done).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AuxiliarySearchProviderBrowserTest,
                       QueryNonSensitiveTab) {
  base::RunLoop run_loop;
  std::unique_ptr<std::vector<TabAndroid*>> tab_vec =
      std::make_unique<std::vector<TabAndroid*>>();
  TabAndroid* first_tab = TabAndroid::FromWebContents(web_contents());
  tab_vec->push_back(first_tab);
  std::unique_ptr<SensitivityPersistedTabDataAndroid> sptda =
      std::make_unique<SensitivityPersistedTabDataAndroid>(first_tab);
  sptda->set_is_sensitive(false);
  first_tab->SetUserData(SensitivityPersistedTabDataAndroid::UserDataKey(),
                         std::move(sptda));

  TabModel* tab_model = TabModelList::GetTabModelForWebContents(web_contents());
  TabAndroid* second_tab = TabAndroid::FromWebContents(web_contents());
  std::unique_ptr<content::WebContents> contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  content::WebContents* second_web_contents = contents.release();
  tab_model->CreateTab(second_tab, second_web_contents);
  std::unique_ptr<SensitivityPersistedTabDataAndroid> sptda2 =
      std::make_unique<SensitivityPersistedTabDataAndroid>(second_tab);
  sptda2->set_is_sensitive(false);
  tab_vec->push_back(second_tab);

  provider()->GetNonSensitiveTabsInternal(
      std::move(tab_vec),
      base::BindOnce(
          [](base::OnceClosure done,
             std::unique_ptr<std::vector<TabAndroid*>> non_sensitive_tab) {
            EXPECT_EQ(2u, non_sensitive_tab->size());
            std::move(done).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AuxiliarySearchProviderBrowserTest, QueryEmptyTabList) {
  base::RunLoop run_loop;
  std::unique_ptr<std::vector<TabAndroid*>> tab_vec =
      std::make_unique<std::vector<TabAndroid*>>();

  provider()->GetNonSensitiveTabsInternal(
      std::move(tab_vec),
      base::BindOnce(
          [](base::OnceClosure done,
             std::unique_ptr<std::vector<TabAndroid*>> non_sensitive_tab) {
            EXPECT_EQ(0u, non_sensitive_tab->size());
            std::move(done).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

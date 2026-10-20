// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites_factory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/grit/locale_settings.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/top_sites.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

namespace {

class TopSitesFactoryTest : public testing::Test {
 protected:
  void SetUp() override {
    test_util_ = std::make_unique<TemplateURLServiceTestUtil>();
    // TemplateURLServiceFactory is null-while-testing; install a real service
    // so that the default search engine can be resolved from `profile()`.
    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        TemplateURLServiceTestUtil::GetTemplateURLServiceTestingFactory());
    // TopSitesFactory is null-while-testing too. Use the default factory so
    // that the prepopulated pages are computed exactly as in production and
    // can be read back through the public history::TopSites API.
    TopSitesFactory::GetInstance()->SetTestingFactory(
        profile(), TopSitesFactory::GetDefaultFactory());
#if BUILDFLAG(IS_ANDROID)
    // Default to the WebUI NTP being enabled for Android.
    scoped_feature_list_.InitAndEnableFeature(
        chrome::android::kUseWebUiNtpAndroid);
#endif
  }

  void TearDown() override {
#if BUILDFLAG(IS_ANDROID)
    base::android::device_info::reset_is_desktop_for_testing();
#endif
    test_util_.reset();
  }

  void SetDefaultSearchProvider(const std::u16string& keyword,
                                const std::string& search_url) {
    std::unique_ptr<TemplateURL> turl =
        CreateTestTemplateURL(keyword, search_url);
    TemplateURL* added_turl = model()->Add(std::move(turl));
    model()->SetUserSelectedDefaultSearchProvider(added_turl);
    test_util_->ChangeModelToLoadState();
  }

  void SetGoogleAsDefaultSearchProvider() {
    SetDefaultSearchProvider(u"google",
                             "https://www.google.com/search?q={searchTerms}");
  }

  void SetBingAsDefaultSearchProvider() {
    SetDefaultSearchProvider(u"bing",
                             "https://www.bing.com/search?q={searchTerms}");
  }

  // Builds the TopSites service for `profile()` (through the production
  // factory path) and returns the prepopulated pages it was built with. Must
  // be called once per test, after the default search provider has been set.
  history::PrepopulatedPageList GetPrepopulatedPages() {
    scoped_refptr<history::TopSites> top_sites =
        TopSitesFactory::GetForProfile(profile());
    CHECK(top_sites);
    return top_sites->GetPrepopulatedPages();
  }

  // Asserts that the prepopulated pages consist of exactly the Chrome Web
  // Store.
  void ExpectWebStoreOnly() {
    history::PrepopulatedPageList pages = GetPrepopulatedPages();
    ASSERT_EQ(1u, pages.size());
    EXPECT_EQ(GURL(l10n_util::GetStringUTF8(IDS_WEBSTORE_URL)),
              pages[0].most_visited.url);
    EXPECT_FALSE(pages[0].most_visited.title.empty());
  }

  TemplateURLService* model() { return test_util_->model(); }
  TestingProfile* profile() { return test_util_->profile(); }

  content::BrowserTaskEnvironment task_environment_;
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif
  std::unique_ptr<TemplateURLServiceTestUtil> test_util_;
};

#if !BUILDFLAG(IS_ANDROID)

TEST_F(TopSitesFactoryTest, Desktop_GoogleDSE_ContainsWebStore) {
  SetGoogleAsDefaultSearchProvider();
  ExpectWebStoreOnly();
}

TEST_F(TopSitesFactoryTest, Desktop_NotGoogleDSE_ContainsWebStore) {
  SetBingAsDefaultSearchProvider();
  ExpectWebStoreOnly();
}

TEST_F(TopSitesFactoryTest, Desktop_HideWebStoreIcon_Empty) {
  SetGoogleAsDefaultSearchProvider();
  profile()->GetPrefs()->SetBoolean(policy::policy_prefs::kHideWebStoreIcon,
                                    true);
  EXPECT_TRUE(GetPrepopulatedPages().empty());
}

#elif BUILDFLAG(ENABLE_EXTENSIONS_CORE)  // Desktop Android.

TEST_F(TopSitesFactoryTest,
       DesktopAndroid_WebUiNtp3PDSEEnabled_NotGoogleDSE_ContainsWebStore) {
  base::android::device_info::set_is_desktop_for_testing(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{chrome::android::kUseWebUiNtp3PDSE},
      /*disabled_features=*/{});
  SetBingAsDefaultSearchProvider();

  ExpectWebStoreOnly();
}

TEST_F(TopSitesFactoryTest,
       DesktopAndroid_WebUiNtp3PDSEEnabled_GoogleDSE_Empty) {
  base::android::device_info::set_is_desktop_for_testing(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{chrome::android::kUseWebUiNtp3PDSE},
      /*disabled_features=*/{});
  SetGoogleAsDefaultSearchProvider();

  EXPECT_TRUE(GetPrepopulatedPages().empty());
}

TEST_F(TopSitesFactoryTest,
       DesktopAndroid_WebUiNtp3PDSEDisabled_NotGoogleDSE_Empty) {
  base::android::device_info::set_is_desktop_for_testing(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{chrome::android::kUseWebUiNtp3PDSE});
  SetBingAsDefaultSearchProvider();

  EXPECT_TRUE(GetPrepopulatedPages().empty());
}

TEST_F(TopSitesFactoryTest,
       DesktopAndroid_WebUiNtpDisabled_NotGoogleDSE_Empty) {
  base::android::device_info::set_is_desktop_for_testing(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{chrome::android::kUseWebUiNtp3PDSE},
      /*disabled_features=*/{chrome::android::kUseWebUiNtpAndroid});
  SetBingAsDefaultSearchProvider();

  EXPECT_TRUE(GetPrepopulatedPages().empty());
}

TEST_F(TopSitesFactoryTest, DesktopAndroid_HideWebStoreIcon_Empty) {
  base::android::device_info::set_is_desktop_for_testing(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{chrome::android::kUseWebUiNtp3PDSE},
      /*disabled_features=*/{});
  SetBingAsDefaultSearchProvider();
  profile()->GetPrefs()->SetBoolean(policy::policy_prefs::kHideWebStoreIcon,
                                    true);

  EXPECT_TRUE(GetPrepopulatedPages().empty());
}

TEST_F(TopSitesFactoryTest, NonDesktopFormFactor_NotGoogleDSE_Empty) {
  base::android::device_info::set_is_desktop_for_testing(false);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{chrome::android::kUseWebUiNtp3PDSE},
      /*disabled_features=*/{});
  SetBingAsDefaultSearchProvider();

  EXPECT_TRUE(GetPrepopulatedPages().empty());
}

#else  // Android builds without the extensions platform (mobile).

TEST_F(TopSitesFactoryTest, MobileAndroid_Empty) {
  // Prepopulated pages are compiled out: even the Desktop Android conditions
  // must not produce any.
  base::android::device_info::set_is_desktop_for_testing(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{chrome::android::kUseWebUiNtp3PDSE},
      /*disabled_features=*/{});
  SetBingAsDefaultSearchProvider();

  EXPECT_TRUE(GetPrepopulatedPages().empty());
}

#endif

}  // namespace

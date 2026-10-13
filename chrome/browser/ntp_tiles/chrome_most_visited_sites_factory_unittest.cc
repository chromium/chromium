// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/tile_source.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

namespace {

class ChromeMostVisitedSitesFactoryTest : public testing::Test {
 protected:
  void SetUp() override {
    test_util_ = std::make_unique<TemplateURLServiceTestUtil>();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        TemplateURLServiceTestUtil::GetTemplateURLServiceTestingFactory());
#if BUILDFLAG(IS_ANDROID)
    // Default to WebUI NTP enabled for Android.
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

  // Helper function to set the default search provider.
  void SetDefaultSearchProvider(const std::u16string& keyword,
                                const std::string& search_url) {
    std::unique_ptr<TemplateURL> turl =
        CreateTestTemplateURL(keyword, search_url);
    TemplateURL* added_turl = model()->Add(std::move(turl));
    model()->SetUserSelectedDefaultSearchProvider(added_turl);
    test_util_->ChangeModelToLoadState();
  }

  // Helper function to check if the popular sites source exists.
  bool DoesPopularSitesSourceExist() {
    std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_sites =
        ChromeMostVisitedSitesFactory::NewForProfile(profile());
    EXPECT_NE(nullptr, most_visited_sites);
    return most_visited_sites &&
           most_visited_sites->DoesSourceExist(ntp_tiles::TileSource::POPULAR);
  }

  TemplateURLService* model() { return test_util_->model(); }
  TestingProfile* profile() { return test_util_->profile(); }

  content::BrowserTaskEnvironment task_environment_;
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif
  std::unique_ptr<TemplateURLServiceTestUtil> test_util_;
};

TEST_F(ChromeMostVisitedSitesFactoryTest, OffTheRecordReturnsNull) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(nullptr, ChromeMostVisitedSitesFactory::NewForProfile(otr_profile));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(ChromeMostVisitedSitesFactoryTest,
       DesktopAndroid_WebUiNtp3PDSEEnabled_GoogleDSE_CreatesPopularSites) {
  // Arrange
  base::android::device_info::set_is_desktop_for_testing(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{chrome::android::kUseWebUiNtp3PDSE},
      /*disabled_features=*/{});
  SetDefaultSearchProvider(u"google",
                           "https://www.google.com/search?q={searchTerms}");

  // Act & Assert
  EXPECT_TRUE(DoesPopularSitesSourceExist());
}

TEST_F(
    ChromeMostVisitedSitesFactoryTest,
    DesktopAndroid_WebUiNtp3PDSEEnabled_NotGoogleDSE_DoesNotCreatePopularSites) {
  // Arrange
  base::android::device_info::set_is_desktop_for_testing(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{chrome::android::kUseWebUiNtp3PDSE},
      /*disabled_features=*/{});
  SetDefaultSearchProvider(u"bing",
                           "https://www.bing.com/search?q={searchTerms}");

  // Act & Assert
  EXPECT_FALSE(DoesPopularSitesSourceExist());
}

TEST_F(ChromeMostVisitedSitesFactoryTest,
       DesktopAndroid_WebUiNtp3PDSEDisabled_CreatesPopularSites) {
  // Arrange
  base::android::device_info::set_is_desktop_for_testing(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{chrome::android::kUseWebUiNtp3PDSE});
  SetDefaultSearchProvider(u"bing",
                           "https://www.bing.com/search?q={searchTerms}");

  // Act & Assert
  EXPECT_TRUE(DoesPopularSitesSourceExist());
}

TEST_F(ChromeMostVisitedSitesFactoryTest,
       DesktopAndroid_WebUiNtpDisabled_CreatesPopularSites) {
  // Arrange
  base::android::device_info::set_is_desktop_for_testing(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{chrome::android::kUseWebUiNtp3PDSE},
      /*disabled_features=*/{chrome::android::kUseWebUiNtpAndroid});
  SetDefaultSearchProvider(u"bing",
                           "https://www.bing.com/search?q={searchTerms}");

  // Act & Assert
  EXPECT_TRUE(DoesPopularSitesSourceExist());
}

TEST_F(ChromeMostVisitedSitesFactoryTest, MobileAndroid_CreatesPopularSites) {
  // Arrange
  base::android::device_info::set_is_desktop_for_testing(false);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{chrome::android::kUseWebUiNtp3PDSE},
      /*disabled_features=*/{});
  SetDefaultSearchProvider(u"bing",
                           "https://www.bing.com/search?q={searchTerms}");

  // Act & Assert
  EXPECT_TRUE(DoesPopularSitesSourceExist());
}
#else
TEST_F(ChromeMostVisitedSitesFactoryTest, DesktopDoesNotCreatePopularSites) {
  // Arrange
  SetDefaultSearchProvider(u"google",
                           "https://www.google.com/search?q={searchTerms}");

  // Act & Assert
  EXPECT_FALSE(DoesPopularSitesSourceExist());
}
#endif
}  // namespace

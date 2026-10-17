// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/new_tab_page_url_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/device_info.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chrome::android {

class NewTabPageUrlHandlerTest : public testing::Test {
 protected:
  NewTabPageUrlHandlerTest() = default;

  void SetUp() override {
    test_util_ = std::make_unique<TemplateURLServiceTestUtil>();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        TemplateURLServiceTestUtil::GetTemplateURLServiceTestingFactory());
  }

  void TearDown() override {
    base::android::device_info::reset_is_desktop_for_testing();
    test_util_.reset();
  }

  TemplateURLService* model() { return test_util_->model(); }
  TestingProfile* profile() { return test_util_->profile(); }

  // Configures the device form factor and the two flags that gate the
  // desktop-Android WebUI NTP: `webui_ntp` (kUseWebUiNtpAndroid) and
  // `third_party_dse` (kUseWebUiNtp3PDSE).
  void ConfigureWebUiNtp(bool is_desktop,
                         bool webui_ntp,
                         bool third_party_dse) {
    base::android::device_info::set_is_desktop_for_testing(is_desktop);
    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;
    (webui_ntp ? enabled : disabled)
        .push_back(chrome::android::kUseWebUiNtpAndroid);
    (third_party_dse ? enabled : disabled)
        .push_back(chrome::android::kUseWebUiNtp3PDSE);
    scoped_feature_list_.InitWithFeatures(enabled, disabled);
  }

  // Installs a search engine as the user-selected default and marks the
  // TemplateURLService as loaded. An empty `new_tab_url` leaves the engine
  // without a remote New Tab page.
  void SetDefaultSearchEngine(const std::u16string& keyword,
                              const std::string& search_url,
                              const std::string& new_tab_url = std::string()) {
    TemplateURLData data;
    data.SetShortName(keyword);
    data.SetKeyword(keyword);
    data.SetURL(search_url);
    data.new_tab_url = new_tab_url;
    TemplateURL* turl = model()->Add(std::make_unique<TemplateURL>(data));
    model()->SetUserSelectedDefaultSearchProvider(turl);
    test_util_->ChangeModelToLoadState();
  }

  void SetGoogleAsDefaultSearchEngine() {
    SetDefaultSearchEngine(u"google",
                           "https://www.google.com/search?q={searchTerms}");
  }

  // Sets a non-Google default search engine, optionally with a remote NTP.
  void SetBingAsDefaultSearchEngine(
      const std::string& new_tab_url = std::string()) {
    SetDefaultSearchEngine(
        u"bing", "https://www.bing.com/search?q={searchTerms}", new_tab_url);
  }

  // Runs chrome://newtab through the URL handler for `profile`, expecting the
  // handler to claim it, and returns the rewritten URL.
  GURL RewriteNewTabUrl(Profile* profile) {
    GURL url(chrome::kChromeUINewTabURL);
    EXPECT_TRUE(HandleAndroidNativePageURL(&url, profile));
    return url;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TemplateURLServiceTestUtil> test_util_;
};

TEST_F(NewTabPageUrlHandlerTest, TestNonSpecialURL) {
  GURL url("http://example.com");
  EXPECT_FALSE(HandleAndroidNativePageURL(&url, profile()));
  EXPECT_EQ(GURL("http://example.com"), url);
}

TEST_F(NewTabPageUrlHandlerTest, TestWebUiNtpRedirection_Enabled_DseGoogle) {
  // Arrange.
  ConfigureWebUiNtp(/*is_desktop=*/true,
                    /*webui_ntp=*/true,
                    /*third_party_dse=*/false);
  SetGoogleAsDefaultSearchEngine();

  // Act and assert.
  EXPECT_EQ(GURL(chrome::kChromeUINewTabPageURL), RewriteNewTabUrl(profile()));
}

TEST_F(NewTabPageUrlHandlerTest,
       TestWebUiNtpRedirection_Enabled_DseGoogle_3pDseEnabled) {
  // Arrange.
  ConfigureWebUiNtp(/*is_desktop=*/true,
                    /*webui_ntp=*/true,
                    /*third_party_dse=*/true);
  SetGoogleAsDefaultSearchEngine();

  // Act and assert.
  EXPECT_EQ(GURL(chrome::kChromeUINewTabPageURL), RewriteNewTabUrl(profile()));
}

TEST_F(NewTabPageUrlHandlerTest, TestWebUiNtpRedirection_Enabled_DseNotGoogle) {
  // Arrange.
  ConfigureWebUiNtp(/*is_desktop=*/true,
                    /*webui_ntp=*/true,
                    /*third_party_dse=*/true);
  SetBingAsDefaultSearchEngine(/*new_tab_url=*/"https://www.bing.com/newtab");

  // Act and assert.
  EXPECT_EQ(GURL("https://www.bing.com/newtab"), RewriteNewTabUrl(profile()));
}

TEST_F(NewTabPageUrlHandlerTest,
       TestWebUiNtpRedirection_Enabled_DseNotGoogle_3pDseDisabled) {
  // Arrange.
  ConfigureWebUiNtp(/*is_desktop=*/true,
                    /*webui_ntp=*/true,
                    /*third_party_dse=*/false);
  SetBingAsDefaultSearchEngine(/*new_tab_url=*/"https://www.bing.com/newtab");

  // Act and assert.
  EXPECT_EQ(GURL(chrome::kChromeUINativeNewTabURL),
            RewriteNewTabUrl(profile()));
}

TEST_F(NewTabPageUrlHandlerTest,
       TestWebUiNtpRedirection_Enabled_DseNotGoogle_NoNewTabUrl) {
  // Arrange.
  ConfigureWebUiNtp(/*is_desktop=*/true,
                    /*webui_ntp=*/true,
                    /*third_party_dse=*/true);
  SetBingAsDefaultSearchEngine();

  // Act and assert.
  EXPECT_EQ(GURL(chrome::kChromeUINewTabPageThirdPartyURL),
            RewriteNewTabUrl(profile()));
}

TEST_F(NewTabPageUrlHandlerTest,
       TestWebUiNtpRedirection_Enabled_DseNotGoogle_InsecureNewTabUrl) {
  // Arrange.
  ConfigureWebUiNtp(/*is_desktop=*/true,
                    /*webui_ntp=*/true,
                    /*third_party_dse=*/true);
  SetBingAsDefaultSearchEngine(/*new_tab_url=*/"http://www.bing.com/newtab");

  // Act and assert.
  EXPECT_EQ(GURL(chrome::kChromeUINewTabPageThirdPartyURL),
            RewriteNewTabUrl(profile()));
}

TEST_F(NewTabPageUrlHandlerTest,
       TestWebUiNtpRedirection_Enabled_DseNotGoogle_3pDseDisabled_NoNewTabUrl) {
  // Arrange.
  ConfigureWebUiNtp(/*is_desktop=*/true,
                    /*webui_ntp=*/true,
                    /*third_party_dse=*/false);
  SetBingAsDefaultSearchEngine();

  // Act and assert.
  EXPECT_EQ(GURL(chrome::kChromeUINativeNewTabURL),
            RewriteNewTabUrl(profile()));
}

TEST_F(NewTabPageUrlHandlerTest,
       TestWebUiNtpRedirection_Enabled_DseNotGoogle_OffTheRecord) {
  // Arrange.
  ConfigureWebUiNtp(/*is_desktop=*/true,
                    /*webui_ntp=*/true,
                    /*third_party_dse=*/true);
  SetBingAsDefaultSearchEngine();
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  // Act and assert.
  EXPECT_EQ(GURL(chrome::kChromeUINativeNewTabURL),
            RewriteNewTabUrl(otr_profile));
}

TEST_F(NewTabPageUrlHandlerTest, TestWebUiNtpRedirection_Disabled) {
  // Arrange.
  ConfigureWebUiNtp(/*is_desktop=*/true,
                    /*webui_ntp=*/false,
                    /*third_party_dse=*/false);
  SetGoogleAsDefaultSearchEngine();

  // Act and assert.
  EXPECT_EQ(GURL(chrome::kChromeUINativeNewTabURL),
            RewriteNewTabUrl(profile()));
}

TEST_F(NewTabPageUrlHandlerTest,
       TestWebUiNtpRedirection_Enabled_DseGoogle_Mobile) {
  // Arrange.
  ConfigureWebUiNtp(/*is_desktop=*/false,
                    /*webui_ntp=*/true,
                    /*third_party_dse=*/false);
  SetGoogleAsDefaultSearchEngine();

  // Act and assert.
  EXPECT_EQ(GURL(chrome::kChromeUINativeNewTabURL),
            RewriteNewTabUrl(profile()));
}

TEST_F(NewTabPageUrlHandlerTest,
       TestWebUiNtpRedirection_Enabled_DseNotGoogle_Mobile) {
  // Arrange.
  ConfigureWebUiNtp(/*is_desktop=*/false,
                    /*webui_ntp=*/true,
                    /*third_party_dse=*/true);
  SetBingAsDefaultSearchEngine(/*new_tab_url=*/"https://www.bing.com/newtab");

  // Act and assert.
  EXPECT_EQ(GURL(chrome::kChromeUINativeNewTabURL),
            RewriteNewTabUrl(profile()));
}

TEST_F(NewTabPageUrlHandlerTest,
       TestWebUiNtpRedirection_Disabled_3pDseEnabled) {
  // Arrange.
  ConfigureWebUiNtp(/*is_desktop=*/true,
                    /*webui_ntp=*/false,
                    /*third_party_dse=*/true);
  SetBingAsDefaultSearchEngine(/*new_tab_url=*/"https://www.bing.com/newtab");

  // Act and assert.
  EXPECT_EQ(GURL(chrome::kChromeUINativeNewTabURL),
            RewriteNewTabUrl(profile()));
}

TEST_F(NewTabPageUrlHandlerTest,
       GetNewTabPageURL_Mobile_DseNotGoogle_NoNewTabUrl_IsEmpty) {
  // Arrange.
  ConfigureWebUiNtp(/*is_desktop=*/false,
                    /*webui_ntp=*/true,
                    /*third_party_dse=*/true);
  SetBingAsDefaultSearchEngine();

  // Act and assert: outside desktop Android there is no WebUI NTP to fall
  // back to, so callers of GetNewTabPageURL() see an invalid URL.
  EXPECT_FALSE(search::GetNewTabPageURL(profile()).is_valid());
}

TEST_F(NewTabPageUrlHandlerTest, HistoryURLRedirection_FeatureEnabled_Desktop) {
  base::android::device_info::set_is_desktop_for_testing(true);
  base::ScopedClosureRunner reset_desktop(base::BindOnce(
      &base::android::device_info::reset_is_desktop_for_testing));
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chrome::android::kAndroidDesktopWebUiHistory);

  GURL url("chrome-native://history");
  EXPECT_TRUE(HandleAndroidNativePageURL(&url, profile()));
  EXPECT_EQ("chrome://history/", url.spec());
}

TEST_F(NewTabPageUrlHandlerTest,
       HistoryURLRedirection_FeatureEnabled_NonDesktop) {
  base::android::device_info::set_is_desktop_for_testing(false);
  base::ScopedClosureRunner reset_desktop(base::BindOnce(
      &base::android::device_info::reset_is_desktop_for_testing));
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chrome::android::kAndroidDesktopWebUiHistory);

  GURL url("chrome-native://history");
  EXPECT_FALSE(HandleAndroidNativePageURL(&url, profile()));
  EXPECT_EQ("chrome-native://history/", url.spec());
}

TEST_F(NewTabPageUrlHandlerTest,
       HistoryURLRedirection_FeatureDisabled_Desktop) {
  base::android::device_info::set_is_desktop_for_testing(true);
  base::ScopedClosureRunner reset_desktop(base::BindOnce(
      &base::android::device_info::reset_is_desktop_for_testing));
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      chrome::android::kAndroidDesktopWebUiHistory);

  GURL url("chrome-native://history");
  EXPECT_FALSE(HandleAndroidNativePageURL(&url, profile()));
  EXPECT_EQ("chrome-native://history/", url.spec());
}

}  // namespace chrome::android

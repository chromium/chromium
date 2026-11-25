// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/new_tab_page_url_handler.h"

#include <memory>
#include <string>
#include <string_view>
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
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/buildflags.h"
#include "url/gurl.h"

namespace chrome::android {

class NewTabPageUrlHandlerTest : public testing::Test {
 protected:
  NewTabPageUrlHandlerTest() = default;
  NewTabPageUrlHandlerTest(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features)
      : scoped_feature_list_(enabled_features, disabled_features) {}

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

#if BUILDFLAG(ENABLE_WEBUI_BOOKMARKS)
class NewTabPageUrlHandlerBookmarksEnabledTest
    : public NewTabPageUrlHandlerTest {
 protected:
  NewTabPageUrlHandlerBookmarksEnabledTest()
      : NewTabPageUrlHandlerTest({chrome::android::kUseWebUiBookmarksAndroid},
                                 {}) {}
};

TEST_F(NewTabPageUrlHandlerBookmarksEnabledTest,
       BookmarksURLRedirection_FeatureEnabled) {
  GURL root_url("chrome-native://bookmarks/");
  EXPECT_TRUE(HandleAndroidNativePageURL(&root_url, profile()));
  EXPECT_EQ("chrome://bookmarks/", root_url.spec());

  GURL ref_url("chrome-native://bookmarks/#123");
  EXPECT_TRUE(HandleAndroidNativePageURL(&ref_url, profile()));
  EXPECT_EQ("chrome://bookmarks/?id=123", ref_url.spec());

  GURL folder_url("chrome-native://bookmarks/folder/123");
  EXPECT_TRUE(HandleAndroidNativePageURL(&folder_url, profile()));
  EXPECT_EQ("chrome://bookmarks/?id=123", folder_url.spec());
  EXPECT_TRUE(HandleAndroidNativePageURLReverse(&folder_url, profile()));

  GURL trailing_slash_folder_url("chrome-native://bookmarks/folder/123/");
  EXPECT_TRUE(
      HandleAndroidNativePageURL(&trailing_slash_folder_url, profile()));
  EXPECT_EQ("chrome://bookmarks/?id=123", trailing_slash_folder_url.spec());

  GURL empty_folder_url("chrome-native://bookmarks/folder/");
  EXPECT_TRUE(HandleAndroidNativePageURL(&empty_folder_url, profile()));
  EXPECT_EQ("chrome://bookmarks/", empty_folder_url.spec());

  GURL empty_folder_with_query_url("chrome-native://bookmarks/folder/?id=123");
  EXPECT_TRUE(
      HandleAndroidNativePageURL(&empty_folder_with_query_url, profile()));
  EXPECT_EQ("chrome://bookmarks/?id=123", empty_folder_with_query_url.spec());

  GURL empty_folder_with_ref_url("chrome-native://bookmarks/folder/#123");
  EXPECT_TRUE(
      HandleAndroidNativePageURL(&empty_folder_with_ref_url, profile()));
  EXPECT_EQ("chrome://bookmarks/?id=123", empty_folder_with_ref_url.spec());

  GURL legacy_chrome_folder_url("chrome://bookmarks/folder/123");
  EXPECT_TRUE(HandleAndroidNativePageURL(&legacy_chrome_folder_url, profile()));
  EXPECT_EQ("chrome://bookmarks/?id=123", legacy_chrome_folder_url.spec());

  GURL legacy_chrome_ref_url("chrome://bookmarks/#123");
  EXPECT_TRUE(HandleAndroidNativePageURL(&legacy_chrome_ref_url, profile()));
  EXPECT_EQ("chrome://bookmarks/?id=123", legacy_chrome_ref_url.spec());

  GURL webui_root_url("chrome://bookmarks/");
  EXPECT_FALSE(HandleAndroidNativePageURL(&webui_root_url, profile()));
  EXPECT_EQ("chrome://bookmarks/", webui_root_url.spec());

  GURL webui_folder_url("chrome://bookmarks/?id=123");
  EXPECT_FALSE(HandleAndroidNativePageURL(&webui_folder_url, profile()));
  EXPECT_EQ("chrome://bookmarks/?id=123", webui_folder_url.spec());

  // Search query `?q=` is preserved in WebUI mode.
  GURL webui_search_url("chrome://bookmarks/?q=abc");
  EXPECT_FALSE(HandleAndroidNativePageURL(&webui_search_url, profile()));
  EXPECT_EQ("chrome://bookmarks/?q=abc", webui_search_url.spec());

  GURL native_search_url("chrome-native://bookmarks/?q=hello+world");
  EXPECT_TRUE(HandleAndroidNativePageURL(&native_search_url, profile()));
  EXPECT_EQ("chrome://bookmarks/?q=hello+world", native_search_url.spec());

  GURL native_folder_and_search_url(
      "chrome-native://bookmarks/folder/123?q=abc");
  EXPECT_TRUE(
      HandleAndroidNativePageURL(&native_folder_and_search_url, profile()));
  EXPECT_EQ("chrome://bookmarks/?id=123&q=abc",
            native_folder_and_search_url.spec());

  // Percent-encoded path/ref IDs are not double-escaped, and dot-segments are
  // rejected.
  GURL encoded_folder_url("chrome-native://bookmarks/folder/123%20456");
  EXPECT_TRUE(HandleAndroidNativePageURL(&encoded_folder_url, profile()));
  EXPECT_EQ("chrome://bookmarks/?id=123+456", encoded_folder_url.spec());

  GURL encoded_slash_folder_url("chrome-native://bookmarks/folder/a%2Fb");
  EXPECT_TRUE(HandleAndroidNativePageURL(&encoded_slash_folder_url, profile()));
  EXPECT_EQ("chrome://bookmarks/?id=a%2Fb", encoded_slash_folder_url.spec());

  GURL encoded_ref_url("chrome-native://bookmarks/#a%20b");
  EXPECT_TRUE(HandleAndroidNativePageURL(&encoded_ref_url, profile()));
  EXPECT_EQ("chrome://bookmarks/?id=a+b", encoded_ref_url.spec());

  GURL dotdot_ref_url("chrome-native://bookmarks/#..");
  EXPECT_TRUE(HandleAndroidNativePageURL(&dotdot_ref_url, profile()));
  EXPECT_EQ("chrome://bookmarks/", dotdot_ref_url.spec());

  GURL dot_ref_url("chrome-native://bookmarks/#.");
  EXPECT_TRUE(HandleAndroidNativePageURL(&dot_ref_url, profile()));
  EXPECT_EQ("chrome://bookmarks/", dot_ref_url.spec());
}

TEST_F(NewTabPageUrlHandlerBookmarksEnabledTest,
       BookmarksURLRedirection_FeatureEnabled_OffTheRecord) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  GURL root_url("chrome-native://bookmarks/");
  EXPECT_TRUE(HandleAndroidNativePageURL(&root_url, otr_profile));
  EXPECT_EQ("chrome://bookmarks/", root_url.spec());

  GURL folder_url("chrome-native://bookmarks/folder/123");
  EXPECT_TRUE(HandleAndroidNativePageURL(&folder_url, otr_profile));
  EXPECT_EQ("chrome://bookmarks/?id=123", folder_url.spec());

  GURL webui_root_url("chrome://bookmarks/");
  EXPECT_FALSE(HandleAndroidNativePageURL(&webui_root_url, otr_profile));
  EXPECT_EQ("chrome://bookmarks/", webui_root_url.spec());
}

TEST_F(NewTabPageUrlHandlerBookmarksEnabledTest,
       BookmarksURLRedirection_FeatureEnabled_GuestProfile) {
  std::unique_ptr<TestingProfile> guest_profile =
      TestingProfile::Builder().SetGuestSession().Build();

  GURL native_url("chrome-native://bookmarks/");
  EXPECT_FALSE(HandleAndroidNativePageURL(&native_url, guest_profile.get()));
  EXPECT_EQ("chrome-native://bookmarks/", native_url.spec());

  GURL webui_url("chrome://bookmarks/?id=123");
  EXPECT_TRUE(HandleAndroidNativePageURL(&webui_url, guest_profile.get()));
  EXPECT_EQ("chrome-native://bookmarks/folder/123", webui_url.spec());
}
#endif  // BUILDFLAG(ENABLE_WEBUI_BOOKMARKS)

class NewTabPageUrlHandlerBookmarksDisabledTest
    : public NewTabPageUrlHandlerTest {
 protected:
  NewTabPageUrlHandlerBookmarksDisabledTest()
      : NewTabPageUrlHandlerTest({},
                                 {chrome::android::kUseWebUiBookmarksAndroid}) {
  }
};

TEST_F(NewTabPageUrlHandlerBookmarksDisabledTest,
       BookmarksURLRedirection_FeatureDisabled) {
  GURL url("chrome-native://bookmarks/");
  EXPECT_FALSE(HandleAndroidNativePageURL(&url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/", url.spec());

  GURL webui_root_url("chrome://bookmarks/");
  EXPECT_TRUE(HandleAndroidNativePageURL(&webui_root_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/", webui_root_url.spec());
  EXPECT_TRUE(HandleAndroidNativePageURLReverse(&webui_root_url, profile()));

  GURL ref_url("chrome-native://bookmarks/#123");
  EXPECT_TRUE(HandleAndroidNativePageURL(&ref_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/folder/123", ref_url.spec());

  GURL webui_folder_url("chrome://bookmarks/?id=123");
  EXPECT_TRUE(HandleAndroidNativePageURL(&webui_folder_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/folder/123", webui_folder_url.spec());
  EXPECT_TRUE(HandleAndroidNativePageURLReverse(&webui_folder_url, profile()));

  GURL canonical_folder_url("chrome-native://bookmarks/folder/123");
  EXPECT_FALSE(HandleAndroidNativePageURL(&canonical_folder_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/folder/123",
            canonical_folder_url.spec());

  // Existing native subpaths without legacy `#<id>` or `?id=<id>` are left
  // untouched when WebUI Bookmarks is disabled.
  GURL uncategorized_url("chrome-native://bookmarks/uncategorized/");
  EXPECT_FALSE(HandleAndroidNativePageURL(&uncategorized_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/uncategorized/",
            uncategorized_url.spec());

  GURL folder_with_extra_query_url("chrome-native://bookmarks/folder/5?x=1");
  EXPECT_FALSE(
      HandleAndroidNativePageURL(&folder_with_extra_query_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/folder/5?x=1",
            folder_with_extra_query_url.spec());

  GURL uppercase_folder_url("chrome-native://bookmarks/FOLDER/5");
  EXPECT_FALSE(HandleAndroidNativePageURL(&uppercase_folder_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/FOLDER/5", uppercase_folder_url.spec());

  GURL trailing_slash_native_folder_url(
      "chrome-native://bookmarks/folder/123/");
  EXPECT_FALSE(
      HandleAndroidNativePageURL(&trailing_slash_native_folder_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/folder/123/",
            trailing_slash_native_folder_url.spec());

  GURL empty_native_folder_url("chrome-native://bookmarks/folder/");
  EXPECT_FALSE(HandleAndroidNativePageURL(&empty_native_folder_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/folder/",
            empty_native_folder_url.spec());

  // Trailing slash and empty `/folder/` edge cases on rewritten URLs.
  GURL trailing_slash_webui_folder_url("chrome://bookmarks/folder/123/");
  EXPECT_TRUE(
      HandleAndroidNativePageURL(&trailing_slash_webui_folder_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/folder/123",
            trailing_slash_webui_folder_url.spec());

  GURL empty_webui_folder_url("chrome://bookmarks/folder/");
  EXPECT_TRUE(HandleAndroidNativePageURL(&empty_webui_folder_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/", empty_webui_folder_url.spec());

  GURL empty_folder_with_query_url("chrome-native://bookmarks/folder/?id=123");
  EXPECT_TRUE(
      HandleAndroidNativePageURL(&empty_folder_with_query_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/folder/123",
            empty_folder_with_query_url.spec());

  GURL empty_folder_with_ref_url("chrome-native://bookmarks/folder/#123");
  EXPECT_TRUE(
      HandleAndroidNativePageURL(&empty_folder_with_ref_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/folder/123",
            empty_folder_with_ref_url.spec());

  // Search query `?q=` (e.g., `@bookmarks <query>`) is preserved when rewriting
  // `chrome://bookmarks/` to `chrome-native://bookmarks/`, while
  // `chrome-native://bookmarks/?q=...` is left untouched when the feature is
  // disabled.
  GURL webui_search_url("chrome://bookmarks/?q=abc");
  EXPECT_TRUE(HandleAndroidNativePageURL(&webui_search_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/?q=abc", webui_search_url.spec());

  GURL webui_multiword_search_url("chrome://bookmarks/?q=hello+world");
  EXPECT_TRUE(
      HandleAndroidNativePageURL(&webui_multiword_search_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/?q=hello+world",
            webui_multiword_search_url.spec());

  GURL native_search_url("chrome-native://bookmarks/?q=abc");
  EXPECT_FALSE(HandleAndroidNativePageURL(&native_search_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/?q=abc", native_search_url.spec());

  GURL webui_folder_and_search_url("chrome://bookmarks/?id=123&q=abc");
  EXPECT_TRUE(
      HandleAndroidNativePageURL(&webui_folder_and_search_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/folder/123?q=abc",
            webui_folder_and_search_url.spec());

  // Encoded path/fragment/query injection payloads are escaped and idempotent;
  // bare dot-segments are rejected.
  GURL traversal_url("chrome://bookmarks/?id=..%2F..%2Ffoo");
  EXPECT_TRUE(HandleAndroidNativePageURL(&traversal_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/folder/..%2F..%2Ffoo",
            traversal_url.spec());
  EXPECT_FALSE(HandleAndroidNativePageURL(&traversal_url, profile()));

  GURL frag_injection_url("chrome://bookmarks/?id=123%23frag");
  EXPECT_TRUE(HandleAndroidNativePageURL(&frag_injection_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/folder/123%23frag",
            frag_injection_url.spec());
  EXPECT_FALSE(frag_injection_url.has_ref());
  EXPECT_FALSE(HandleAndroidNativePageURL(&frag_injection_url, profile()));

  GURL query_injection_url("chrome://bookmarks/?id=123%3Fx%3D1");
  EXPECT_TRUE(HandleAndroidNativePageURL(&query_injection_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/folder/123%3Fx%3D1",
            query_injection_url.spec());
  EXPECT_FALSE(query_injection_url.has_query());
  EXPECT_FALSE(HandleAndroidNativePageURL(&query_injection_url, profile()));

  GURL ref_query_injection_url("chrome-native://bookmarks/#123?x=1");
  EXPECT_TRUE(HandleAndroidNativePageURL(&ref_query_injection_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/folder/123%3Fx%3D1",
            ref_query_injection_url.spec());
  EXPECT_FALSE(ref_query_injection_url.has_query());
  EXPECT_FALSE(HandleAndroidNativePageURL(&ref_query_injection_url, profile()));

  GURL dotdot_query_url("chrome://bookmarks/?id=..");
  EXPECT_TRUE(HandleAndroidNativePageURL(&dotdot_query_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/", dotdot_query_url.spec());

  GURL dot_query_url("chrome://bookmarks/?id=.");
  EXPECT_TRUE(HandleAndroidNativePageURL(&dot_query_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/", dot_query_url.spec());

  GURL dotdot_ref_url("chrome-native://bookmarks/#..");
  EXPECT_TRUE(HandleAndroidNativePageURL(&dotdot_ref_url, profile()));
  EXPECT_EQ("chrome-native://bookmarks/", dotdot_ref_url.spec());
}

TEST_F(NewTabPageUrlHandlerTest, HandleAndroidNativePageURLReverse) {
  constexpr std::string_view kMatchingUrls[] = {
      "chrome://bookmarks/",        "chrome://bookmarks/?id=123",
      "chrome-native://bookmarks/", "chrome-native://bookmarks/folder/123",
      "chrome://history/",          "chrome-native://history/",
  };
  for (std::string_view url_spec : kMatchingUrls) {
    GURL url(url_spec);
    EXPECT_TRUE(HandleAndroidNativePageURLReverse(&url, profile())) << url_spec;
    EXPECT_EQ(url_spec, url.spec());
  }

  constexpr std::string_view kNonMatchingUrls[] = {
      "chrome://newtab/",           "chrome-native://newtab/",
      "chrome://new-tab-page/",     "https://example.com/",
      "chrome-native://downloads/",
  };
  for (std::string_view url_spec : kNonMatchingUrls) {
    GURL url(url_spec);
    EXPECT_FALSE(HandleAndroidNativePageURLReverse(&url, profile()))
        << url_spec;
    EXPECT_EQ(url_spec, url.spec());
  }
}

}  // namespace chrome::android

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/new_tab_page_url_handler.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/search_engines/template_url.h"
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

  void TearDown() override { test_util_.reset(); }

  TemplateURLService* model() { return test_util_->model(); }
  TestingProfile* profile() { return test_util_->profile(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TemplateURLServiceTestUtil> test_util_;
};

TEST_F(NewTabPageUrlHandlerTest, TestNonSpecialURL) {
  // Arrange.
  GURL url("http://example.com");

  // Act and assert.
  EXPECT_FALSE(HandleAndroidNativePageURL(&url, profile()));
  EXPECT_EQ("http://example.com/", url.spec());
}

TEST_F(NewTabPageUrlHandlerTest, TestWebUiNtpRedirection_Enabled_DseGoogle) {
  // Arrange.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chrome::android::kUseWebUiNtpAndroid);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kUseWebUiNtp);

  // Mock Google DSE setup
  std::unique_ptr<TemplateURL> google_turl = CreateTestTemplateURL(
      u"google", "http://www.google.com/search?q={searchTerms}");
  TemplateURL* added_turl = model()->Add(std::move(google_turl));
  model()->SetUserSelectedDefaultSearchProvider(added_turl);
  test_util_->ChangeModelToLoadState();

  GURL url(chrome::kChromeUINewTabURL);

  // Act and assert.
  EXPECT_TRUE(HandleAndroidNativePageURL(&url, profile()));
  EXPECT_EQ(chrome::kChromeUINewTabPageURL, url.spec());
}

TEST_F(NewTabPageUrlHandlerTest, TestWebUiNtpRedirection_Enabled_DseNotGoogle) {
  // Arrange.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chrome::android::kUseWebUiNtpAndroid);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kUseWebUiNtp);

  // DSE is NOT Google
  std::unique_ptr<TemplateURL> bing_turl = CreateTestTemplateURL(
      u"bing", "http://www.bing.com/search?q={searchTerms}");
  TemplateURL* added_turl = model()->Add(std::move(bing_turl));
  model()->SetUserSelectedDefaultSearchProvider(added_turl);
  test_util_->ChangeModelToLoadState();

  GURL url(chrome::kChromeUINewTabURL);

  // Act and assert.
  EXPECT_TRUE(HandleAndroidNativePageURL(&url, profile()));
  EXPECT_EQ(chrome::kChromeUINativeNewTabURL, url.spec());
}

TEST_F(NewTabPageUrlHandlerTest, TestWebUiNtpRedirection_Disabled) {
  // Arrange.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      chrome::android::kUseWebUiNtpAndroid);

  // Set DSE to Google.
  std::unique_ptr<TemplateURL> google_turl = CreateTestTemplateURL(
      u"google", "http://www.google.com/search?q={searchTerms}");
  TemplateURL* added_turl = model()->Add(std::move(google_turl));
  model()->SetUserSelectedDefaultSearchProvider(added_turl);
  test_util_->ChangeModelToLoadState();

  GURL url(chrome::kChromeUINewTabURL);

  // Act and assert.
  EXPECT_TRUE(HandleAndroidNativePageURL(&url, profile()));
  EXPECT_EQ(chrome::kChromeUINativeNewTabURL, url.spec());
}

}  // namespace chrome::android

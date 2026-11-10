// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_sharing_utils.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#endif

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#endif

namespace glic {

class GlicSharingUtilsTest : public testing::Test {
 public:
  GlicSharingUtilsTest() = default;

  content::WebContents* CreateWebContents(const GURL& url) {
    content::WebContents* web_contents =
        web_contents_factory_.CreateWebContents(&profile_);
    content::WebContentsTester::For(web_contents)->NavigateAndCommit(url);
    return web_contents;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
};

TEST_F(GlicSharingUtilsTest, IsTabValidForSharing_HttpsUrl) {
  content::WebContents* web_contents =
      CreateWebContents(GURL("https://example.com"));
  EXPECT_TRUE(IsTabValidForSharing(web_contents));
}

TEST_F(GlicSharingUtilsTest, IsTabValidForSharing_FileUrl) {
  content::WebContents* web_contents =
      CreateWebContents(GURL("file:///tmp/test.html"));
  EXPECT_TRUE(IsTabValidForSharing(web_contents));
}

TEST_F(GlicSharingUtilsTest, IsTabValidForSharing_ChromeUINewTab) {
  content::WebContents* web_contents =
      CreateWebContents(chrome::ChromeUINewTabURLAsGURL());
  EXPECT_TRUE(IsTabValidForSharing(web_contents));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(GlicSharingUtilsTest, IsTabValidForSharing_ChromeUINativeNewTab) {
  content::WebContents* web_contents =
      CreateWebContents(GURL(chrome::kChromeUINativeNewTabURL));
  EXPECT_TRUE(IsTabValidForSharing(web_contents));
}
#endif

TEST_F(GlicSharingUtilsTest, IsTabValidForSharing_InvalidSharingPage) {
  content::WebContents* web_contents =
      CreateWebContents(GURL(chrome::kChromeUIAboutURL));
  EXPECT_FALSE(IsTabValidForSharing(web_contents));
}

TEST_F(GlicSharingUtilsTest, IsTabValidForSharing_TabInterface_HttpsUrl) {
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetURL())
      .WillRepeatedly(testing::Return(GURL("https://example.com")));
  EXPECT_TRUE(IsTabValidForSharing(&mock_tab));
}

TEST_F(GlicSharingUtilsTest, IsTabValidForSharing_TabInterface_InvalidUrl) {
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetURL())
      .WillRepeatedly(testing::Return(GURL(chrome::kChromeUIAboutURL)));
  EXPECT_FALSE(IsTabValidForSharing(&mock_tab));
}

TEST_F(GlicSharingUtilsTest, IsContextHubTopicUrl) {
  EXPECT_TRUE(IsContextHubTopicUrl(GURL("chrome://context-hub/topics")));
  EXPECT_TRUE(IsContextHubTopicUrl(GURL("chrome://context-hub/topic_details")));
  EXPECT_TRUE(
      IsContextHubTopicUrl(GURL("chrome://context-hub/topic_details?id=123")));
  EXPECT_TRUE(
      IsContextHubTopicUrl(GURL("chrome://context-hub/topic_details.html")));
  EXPECT_TRUE(IsContextHubTopicUrl(
      GURL("chrome://context-hub/topic_details.html?id=123")));

  // Non-matching paths, hosts, or schemes.
  EXPECT_FALSE(IsContextHubTopicUrl(GURL("chrome://context-hub")));
  EXPECT_FALSE(IsContextHubTopicUrl(GURL("chrome://context-hub/")));
  EXPECT_FALSE(
      IsContextHubTopicUrl(GURL("chrome://context-hub/save_to_memory_bank")));
  EXPECT_FALSE(IsContextHubTopicUrl(GURL("chrome://context-hub/invalid")));
  EXPECT_FALSE(IsContextHubTopicUrl(GURL("https://context-hub/topics")));
  EXPECT_FALSE(IsContextHubTopicUrl(GURL("chrome://other-host/topics")));
}

TEST_F(GlicSharingUtilsTest, IsTabValidForSharing_ContextHubTopicUrls) {
  content::WebContents* topics_contents =
      CreateWebContents(GURL("chrome://context-hub/topics"));
  EXPECT_TRUE(IsTabValidForSharing(topics_contents));

  content::WebContents* details_contents =
      CreateWebContents(GURL("chrome://context-hub/topic_details?id=123"));
  EXPECT_TRUE(IsTabValidForSharing(details_contents));

  content::WebContents* details_html_contents =
      CreateWebContents(GURL("chrome://context-hub/topic_details.html?id=123"));
  EXPECT_TRUE(IsTabValidForSharing(details_html_contents));

  content::WebContents* invalid_hub_contents =
      CreateWebContents(GURL("chrome://context-hub/save_to_memory_bank"));
  EXPECT_FALSE(IsTabValidForSharing(invalid_hub_contents));
}

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
TEST_F(GlicSharingUtilsTest,
       IsScreenshotBlockedByEnterprisePolicy_DataControls) {
  content::WebContents* blocked_contents =
      CreateWebContents(GURL("https://blocked.example.com/page"));
  content::WebContents* allowed_contents =
      CreateWebContents(GURL("https://allowed.example.com/page"));

  EXPECT_FALSE(IsScreenshotBlockedByEnterprisePolicy(blocked_contents));
  EXPECT_FALSE(IsScreenshotBlockedByEnterprisePolicy(allowed_contents));

  data_controls::SetDataControls(profile_.GetPrefs(), {R"({
    "sources": {
      "urls": ["blocked.example.com"]
    },
    "restrictions": [
      {"class": "SCREENSHOT", "level": "BLOCK"}
    ]
  })"});

  EXPECT_TRUE(IsScreenshotBlockedByEnterprisePolicy(blocked_contents));
  EXPECT_FALSE(IsScreenshotBlockedByEnterprisePolicy(allowed_contents));
  EXPECT_FALSE(IsScreenshotBlockedByEnterprisePolicy(nullptr));

  // Non-SCREENSHOT restrictions or non-BLOCK levels do not block screenshots.
  data_controls::SetDataControls(profile_.GetPrefs(), {R"({
    "sources": {
      "urls": ["blocked.example.com"]
    },
    "restrictions": [
      {"class": "CLIPBOARD", "level": "BLOCK"},
      {"class": "SCREENSHOT", "level": "WARN"}
    ]
  })"});
  EXPECT_FALSE(IsScreenshotBlockedByEnterprisePolicy(blocked_contents));
}
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
TEST_F(GlicSharingUtilsTest,
       IsScreenshotBlockedByEnterprisePolicy_NavigationController) {
  content::WebContents* web_contents =
      CreateWebContents(GURL("https://example.com/page"));
  tabs::MockTabInterface tab;
  ON_CALL(tab, GetContents()).WillByDefault(testing::Return(web_contents));
  // Let `DataProtectionNavigationController::FromWebContents()` resolve the
  // tab, as `TabModel` / `TabAndroid` do for real tabs.
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents, &tab);
  enterprise_data_protection::DataProtectionNavigationController controller(
      &tab);
  ASSERT_EQ(&controller,
            enterprise_data_protection::DataProtectionNavigationController::
                FromWebContents(web_contents));

  EXPECT_FALSE(IsScreenshotBlockedByEnterprisePolicy(web_contents));

  // The cached navigation-time verdict blocks screenshots even when no
  // DataControlsRules SCREENSHOT rule matches the URL.
  controller.SetScreenshotAllowedForTesting(false);
  EXPECT_TRUE(IsScreenshotBlockedByEnterprisePolicy(web_contents));

  controller.SetScreenshotAllowedForTesting(true);
  EXPECT_FALSE(IsScreenshotBlockedByEnterprisePolicy(web_contents));
}
#endif  // BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)

TEST_F(GlicSharingUtilsTest, ShouldPinOnBind_SettingDisabledDefaultsTrue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicDefaultTabContextSetting);

  profile_.GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled, false);
  EXPECT_TRUE(ShouldPinOnBind(&profile_));
  EXPECT_TRUE(ShouldPinOnBind(nullptr));
}

TEST_F(GlicSharingUtilsTest, ShouldPinOnBind_SettingEnabledRespectsPref) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kGlicDefaultTabContextSetting,
                                 features::kGlicDefaultContextPinOnBind},
                                {});

  profile_.GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled, true);
  EXPECT_TRUE(ShouldPinOnBind(&profile_));

  profile_.GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled, false);
  EXPECT_FALSE(ShouldPinOnBind(&profile_));
  EXPECT_FALSE(ShouldPinOnBind(nullptr));
}

TEST_F(GlicSharingUtilsTest, ShouldPinOnBind_KillSwitchDisabledReturnsFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kGlicDefaultTabContextSetting},
                                {features::kGlicDefaultContextPinOnBind});

  profile_.GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled, true);
  EXPECT_FALSE(ShouldPinOnBind(&profile_));
}
}  // namespace glic

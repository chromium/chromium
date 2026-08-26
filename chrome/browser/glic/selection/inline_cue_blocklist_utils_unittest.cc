// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/inline_cue_blocklist_utils.h"

#include <algorithm>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {

class InlineCueBlocklistUtilsTest : public testing::Test {
 public:
  InlineCueBlocklistUtilsTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicSelectionPrompt,
        {{features::kGlicSelectionDefaultBlockedSites.name,
          "https://blocked-site-a.com,https://blocked-site-b.com"}});
  }
  ~InlineCueBlocklistUtilsTest() override = default;

  TestingProfile* profile() { return &profile_; }
  HostContentSettingsMap* settings_map() {
    return HostContentSettingsMapFactory::GetForProfile(&profile_);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(InlineCueBlocklistUtilsTest, IsSiteInDefaultBlocklistForInlineCue) {
  EXPECT_TRUE(
      IsSiteInDefaultBlocklistForInlineCue("https://blocked-site-a.com"));
  EXPECT_TRUE(
      IsSiteInDefaultBlocklistForInlineCue("https://blocked-site-b.com"));
  EXPECT_FALSE(
      IsSiteInDefaultBlocklistForInlineCue("https://allowed-site.com"));
}

TEST_F(InlineCueBlocklistUtilsTest, IsSiteBlockedByDefault) {
  EXPECT_TRUE(IsSiteBlockedForInlineCue(
      profile(), GURL("https://blocked-site-a.com/page1")));
  EXPECT_FALSE(IsSiteBlockedForInlineCue(
      profile(), GURL("https://allowed-site.com/page1")));
}

TEST_F(InlineCueBlocklistUtilsTest, UserExplicitBlockTakesPrecedence) {
  GURL allowed_url("https://allowed-site.com/page1");
  EXPECT_FALSE(IsSiteBlockedForInlineCue(profile(), allowed_url));

  settings_map()->SetContentSettingDefaultScope(
      allowed_url, allowed_url, ContentSettingsType::INLINE_CUE_MENU,
      CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(IsSiteBlockedForInlineCue(profile(), allowed_url));
}

TEST_F(InlineCueBlocklistUtilsTest, UnblockDefaultSite) {
  GURL blocked_url("https://blocked-site-a.com/page1");
  EXPECT_TRUE(IsSiteBlockedForInlineCue(profile(), blocked_url));

  EXPECT_TRUE(
      UnblockDefaultSiteForInlineCue(profile(), "https://blocked-site-a.com"));
  EXPECT_FALSE(IsSiteBlockedForInlineCue(profile(), blocked_url));

  EXPECT_FALSE(
      UnblockDefaultSiteForInlineCue(profile(), "https://allowed-site.com"));
}

TEST_F(InlineCueBlocklistUtilsTest,
       GetActiveDefaultBlockedSitePatternsForInlineCue) {
  std::vector<std::string> active_sites =
      GetActiveDefaultBlockedSitePatternsForInlineCue(profile());
  EXPECT_EQ(active_sites.size(), 2u);
  EXPECT_TRUE(std::ranges::find(active_sites, "https://blocked-site-a.com") !=
              active_sites.end());
  EXPECT_TRUE(std::ranges::find(active_sites, "https://blocked-site-b.com") !=
              active_sites.end());

  EXPECT_TRUE(
      UnblockDefaultSiteForInlineCue(profile(), "https://blocked-site-a.com"));

  EXPECT_EQ(GetActiveDefaultBlockedSitePatternsForInlineCue(profile()),
            std::vector<std::string>{"https://blocked-site-b.com"});
}

}  // namespace glic

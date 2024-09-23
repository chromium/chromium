// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr char kCheckPermission[] = R"(
  (async () => {
    return (await navigator.permissions.query({name:'local-fonts'})).state;
  })();
)";

class PolicyTestLocalFonts : public PolicyTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "FontAccess");
    PolicyTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(PolicyTestLocalFonts, DefaultSetting) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::LOCAL_FONTS, nullptr));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::LOCAL_FONTS));
  EXPECT_EQ("prompt", EvalJs(tab, kCheckPermission));

  // Update policy to change the default permission value to 'block'.
  PolicyMap policies;
  SetPolicy(&policies, key::kDefaultLocalFontsSetting, base::Value(2));
  UpdateProviderPolicy(policies);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::LOCAL_FONTS, nullptr));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::LOCAL_FONTS));
  EXPECT_EQ("denied", EvalJs(tab, kCheckPermission));

  // Update policy to change the default permission value to 'ask'.
  SetPolicy(&policies, key::kDefaultLocalFontsSetting, base::Value(3));
  UpdateProviderPolicy(policies);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::LOCAL_FONTS, nullptr));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::LOCAL_FONTS));
  EXPECT_EQ("prompt", EvalJs(tab, kCheckPermission));
}

IN_PROC_BROWSER_TEST_F(PolicyTestLocalFonts, AllowedForUrlsSettings) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  PolicyMap policies;
  base::Value::List list;
  list.Append(url.spec());
  SetPolicy(&policies, key::kLocalFontsAllowedForUrls,
            base::Value(std::move(list)));
  UpdateProviderPolicy(policies);

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::LOCAL_FONTS, nullptr));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::LOCAL_FONTS));
  EXPECT_EQ("granted", EvalJs(tab, kCheckPermission));
}

IN_PROC_BROWSER_TEST_F(PolicyTestLocalFonts, BlockedForUrlsSettings) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  PolicyMap policies;
  base::Value::List list;
  list.Append(url.spec());
  SetPolicy(&policies, key::kLocalFontsBlockedForUrls,
            base::Value(std::move(list)));
  UpdateProviderPolicy(policies);

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::LOCAL_FONTS, nullptr));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::LOCAL_FONTS));
  EXPECT_EQ("denied", EvalJs(tab, kCheckPermission));
}

}  // namespace

}  // namespace policy

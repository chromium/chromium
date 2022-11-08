// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr char kGetScreens[] = R"(
  (async () => {
    try {
      const screenDetails = await self.getScreenDetails();
    } catch {
      return 'error';
    }
    return (await navigator.permissions.query({name:'window-placement'})).state;
  })();
)";

constexpr char kCheckPermission[] = R"(
  (async () => {
    return (await navigator.permissions.query({name:'window-placement'})).state;
  })();
)";

class PolicyTestWindowManagement : public PolicyTest {};

IN_PROC_BROWSER_TEST_F(PolicyTestWindowManagement, DefaultSetting) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::WINDOW_MANAGEMENT, nullptr));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::WINDOW_MANAGEMENT));
  EXPECT_EQ("prompt", EvalJs(tab, kCheckPermission));

  PolicyMap policies;
  SetPolicy(&policies, key::kDefaultWindowPlacementSetting, base::Value(2));
  UpdateProviderPolicy(policies);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::WINDOW_MANAGEMENT, nullptr));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::WINDOW_MANAGEMENT));
  EXPECT_EQ("denied", EvalJs(tab, kCheckPermission));
  EXPECT_EQ("error", EvalJs(tab, kGetScreens));

  SetPolicy(&policies, key::kDefaultWindowPlacementSetting, base::Value(3));
  UpdateProviderPolicy(policies);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::WINDOW_MANAGEMENT, nullptr));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::WINDOW_MANAGEMENT));
  EXPECT_EQ("prompt", EvalJs(tab, kCheckPermission));
}

IN_PROC_BROWSER_TEST_F(PolicyTestWindowManagement, AllowedForUrlsSettings) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  PolicyMap policies;
  base::Value list(base::Value::Type::LIST);
  list.Append(url.spec());
  SetPolicy(&policies, key::kWindowPlacementAllowedForUrls, std::move(list));
  UpdateProviderPolicy(policies);

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::WINDOW_MANAGEMENT, nullptr));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::WINDOW_MANAGEMENT));
  EXPECT_EQ("granted", EvalJs(tab, kCheckPermission));
  EXPECT_EQ("granted", EvalJs(tab, kGetScreens));
}

IN_PROC_BROWSER_TEST_F(PolicyTestWindowManagement, BlockedForUrlsSettings) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  PolicyMap policies;
  base::Value list(base::Value::Type::LIST);
  list.Append(url.spec());
  SetPolicy(&policies, key::kWindowPlacementBlockedForUrls, std::move(list));
  UpdateProviderPolicy(policies);

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::WINDOW_MANAGEMENT, nullptr));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::WINDOW_MANAGEMENT));
  EXPECT_EQ("denied", EvalJs(tab, kCheckPermission));
  EXPECT_EQ("error", EvalJs(tab, kGetScreens));
}

}  // namespace

}  // namespace policy

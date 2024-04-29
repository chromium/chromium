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
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr char kGetScreensScript[] = R"(
  (async () => {
    try {
      const screenDetails = await self.getScreenDetails();
    } catch {
      return 'error';
    }
    try {
      return (await navigator.permissions.query({name:'window-management'}))
              .state;
    } catch {
      return "permission_error";
    }
  })();
)";

constexpr char kCheckPermissionScript[] = R"(
  (async () => {
    try {
      return (await navigator.permissions.query({name:'window-management'}))
              .state;
     } catch {
      return 'permission_error';
    }
  })();
)";

struct PolicySet {
  const char* default_setting;
  const char* allowed_for_urls_setting;
  const char* blocked_for_urls_setting;
};

class PolicyTestWindowManagement
    : public PolicyTest,
      public testing::WithParamInterface<PolicySet> {
 public:
 protected:
  const PolicySet& PolicySet() const { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(PolicyTestWindowManagement, DefaultSetting) {
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

  EXPECT_EQ("prompt", EvalJs(tab, kCheckPermissionScript));

  PolicyMap policies;
  SetPolicy(&policies, PolicySet().default_setting, base::Value(2));
  UpdateProviderPolicy(policies);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::WINDOW_MANAGEMENT, nullptr));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::WINDOW_MANAGEMENT));

  EXPECT_EQ("denied", EvalJs(tab, kCheckPermissionScript));
  EXPECT_EQ("error", EvalJs(tab, kGetScreensScript));

  SetPolicy(&policies, PolicySet().default_setting, base::Value(3));
  UpdateProviderPolicy(policies);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::WINDOW_MANAGEMENT, nullptr));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::WINDOW_MANAGEMENT));

  EXPECT_EQ("prompt", EvalJs(tab, kCheckPermissionScript));
}

IN_PROC_BROWSER_TEST_P(PolicyTestWindowManagement, AllowedForUrlsSettings) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  PolicyMap policies;
  base::Value::List list;
  list.Append(url.spec());
  SetPolicy(&policies, PolicySet().allowed_for_urls_setting,
            base::Value(std::move(list)));
  UpdateProviderPolicy(policies);

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::WINDOW_MANAGEMENT, nullptr));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::WINDOW_MANAGEMENT));
  EXPECT_EQ("granted", EvalJs(tab, kCheckPermissionScript));
  EXPECT_EQ("granted", EvalJs(tab, kGetScreensScript));
}

IN_PROC_BROWSER_TEST_P(PolicyTestWindowManagement, BlockedForUrlsSettings) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  PolicyMap policies;
  base::Value::List list;
  list.Append(url.spec());
  SetPolicy(&policies, PolicySet().blocked_for_urls_setting,
            base::Value(std::move(list)));
  UpdateProviderPolicy(policies);

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::WINDOW_MANAGEMENT, nullptr));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::WINDOW_MANAGEMENT));
  EXPECT_EQ("denied", EvalJs(tab, kCheckPermissionScript));
  EXPECT_EQ("error", EvalJs(tab, kGetScreensScript));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PolicyTestWindowManagement,
    ::testing::Values(PolicySet{key::kDefaultWindowPlacementSetting,
                                key::kWindowPlacementAllowedForUrls,
                                key::kWindowPlacementBlockedForUrls},
                      PolicySet{key::kDefaultWindowManagementSetting,
                                key::kWindowManagementAllowedForUrls,
                                key::kWindowManagementBlockedForUrls}));
}  // namespace

}  // namespace policy

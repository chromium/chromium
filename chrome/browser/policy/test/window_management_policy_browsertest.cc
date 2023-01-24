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
#include "components/permissions/features.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

// Test both aliases during migration. See crbug.com/1328581.
constexpr char kOldPermissionName[] = "window-placement";
constexpr char kNewPermissionName[] = "window-management";

constexpr char kGetScreensTemplate[] = R"(
  (async () => {
    try {
      const screenDetails = await self.getScreenDetails();
    } catch {
      return 'error';
    }
    try {
      return (await navigator.permissions.query({name:'$1'})).state;
    } catch {
      return "permission_error";
    }
  })();
)";

constexpr char kCheckPermissionTemplate[] = R"(
  (async () => {
    try {
      return (await navigator.permissions.query({name:'$1'})).state;
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

typedef std::tuple<bool, bool, PolicySet> PolicyTestParams;

class PolicyTestWindowManagement
    : public PolicyTest,
      public testing::WithParamInterface<PolicyTestParams> {
 public:
  PolicyTestWindowManagement() {
    scoped_feature_list_.InitWithFeatureState(
        permissions::features::kWindowManagementPermissionAlias,
        AliasEnabled());
  }

 protected:
  bool AliasEnabled() const { return std::get<0>(GetParam()); }
  bool UseAlias() const { return std::get<1>(GetParam()); }
  const PolicySet& PolicySet() const { return std::get<2>(GetParam()); }
  bool ShouldError() const { return UseAlias() && !AliasEnabled(); }
  std::string GetScreensScript() const {
    return base::ReplaceStringPlaceholders(
        kGetScreensTemplate,
        {UseAlias() ? kNewPermissionName : kOldPermissionName}, nullptr);
  }
  std::string GetCheckPermissionScript() const {
    return base::ReplaceStringPlaceholders(
        kCheckPermissionTemplate,
        {UseAlias() ? kNewPermissionName : kOldPermissionName}, nullptr);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

  // Should error if and only if alias is used but flag is not enabled.
  EXPECT_EQ(ShouldError() ? "permission_error" : "prompt",
            EvalJs(tab, GetCheckPermissionScript()));

  PolicyMap policies;
  SetPolicy(&policies, PolicySet().default_setting, base::Value(2));
  UpdateProviderPolicy(policies);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::WINDOW_MANAGEMENT, nullptr));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::WINDOW_MANAGEMENT));
  // Should error if alias is used but flag is not enabled.
  EXPECT_EQ(ShouldError() ? "permission_error" : "denied",
            EvalJs(tab, GetCheckPermissionScript()));
  EXPECT_EQ("error", EvalJs(tab, GetScreensScript()));

  SetPolicy(&policies, PolicySet().default_setting, base::Value(3));
  UpdateProviderPolicy(policies);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::WINDOW_MANAGEMENT, nullptr));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::WINDOW_MANAGEMENT));

  // Should error if and only if alias is used but flag is not enabled.
  EXPECT_EQ(ShouldError() ? "permission_error" : "prompt",
            EvalJs(tab, GetCheckPermissionScript()));
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
  // Should error if and only if alias is used but flag is not enabled.
  std::string expect_str = ShouldError() ? "permission_error" : "granted";
  EXPECT_EQ(expect_str, EvalJs(tab, GetCheckPermissionScript()));
  EXPECT_EQ(expect_str, EvalJs(tab, GetScreensScript()));
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
  // Should error if alias is used but flag is not enabled.
  EXPECT_EQ(ShouldError() ? "permission_error" : "denied",
            EvalJs(tab, GetCheckPermissionScript()));
  EXPECT_EQ("error", EvalJs(tab, GetScreensScript()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PolicyTestWindowManagement,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Values(PolicySet{key::kDefaultWindowPlacementSetting,
                                    key::kWindowPlacementAllowedForUrls,
                                    key::kWindowPlacementBlockedForUrls},
                          PolicySet{key::kDefaultWindowManagementSetting,
                                    key::kWindowManagementAllowedForUrls,
                                    key::kWindowManagementBlockedForUrls})));
}  // namespace

}  // namespace policy

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

class PolicyTestAutomaticFullscreen : public PolicyTest {
 public:
  content::EvalJsResult FullscreenWithoutGesture() {
    constexpr char kScript[] = R"(
      (async () => {
        if (navigator.userActivation.isActive)
          throw new Error('Unexpected user activation');
        return document.body.requestFullscreen();
      })();
    )";
    auto* tab = chrome_test_utils::GetActiveWebContents(this);
    return EvalJs(tab, kScript, content::EXECUTE_SCRIPT_NO_USER_GESTURE);
  }

  ContentSetting GetDefaultContentSetting() {
    return GetHostContentSettingsMap()->GetDefaultContentSetting(
        ContentSettingsType::AUTOMATIC_FULLSCREEN, nullptr);
  }

  ContentSetting GetContentSetting(const GURL& url) {
    return GetHostContentSettingsMap()->GetContentSetting(
        url, url, ContentSettingsType::AUTOMATIC_FULLSCREEN);
  }

  HostContentSettingsMap* GetHostContentSettingsMap() {
    return HostContentSettingsMapFactory::GetForProfile(
        chrome_test_utils::GetProfile(this));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutomaticFullscreenContentSetting};
};

IN_PROC_BROWSER_TEST_F(PolicyTestAutomaticFullscreen, Default) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(NavigateToUrl(url, this));

  // Fullscreen transient activation requirements are enforced by default.
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetDefaultContentSetting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(url));
  EXPECT_THAT(FullscreenWithoutGesture(), content::EvalJsResult::IsError());
}

IN_PROC_BROWSER_TEST_F(PolicyTestAutomaticFullscreen, AllowedForUrls) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));

  PolicyMap policies;
  base::Value::List list;
  list.Append(url.spec());
  SetPolicy(&policies, key::kAutomaticFullscreenAllowedForUrls,
            base::Value(std::move(list)));
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(NavigateToUrl(url, this));

  // Fullscreen transient activation requirements are waived for this origin.
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetDefaultContentSetting());
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetContentSetting(url));
  EXPECT_THAT(FullscreenWithoutGesture(), content::EvalJsResult::IsOk());
}

IN_PROC_BROWSER_TEST_F(PolicyTestAutomaticFullscreen, BlockedForUrls) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));

  PolicyMap policies;
  base::Value::List list;
  list.Append(url.spec());
  SetPolicy(&policies, key::kAutomaticFullscreenBlockedForUrls,
            base::Value(std::move(list)));
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(NavigateToUrl(url, this));

  // Fullscreen transient activation requirements are enforced for this origin.
  EXPECT_THAT(FullscreenWithoutGesture(), content::EvalJsResult::IsError());
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetDefaultContentSetting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(url));
}

}  // namespace policy

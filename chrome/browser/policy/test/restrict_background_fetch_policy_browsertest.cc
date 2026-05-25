// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

namespace policy {

enum class Policy {
  kDefault,
  kTrue,
  kFalse,
};

class RestrictBackgroundFetchPolicyBrowserTest
    : public PolicyTest,
      public ::testing::WithParamInterface<Policy> {
 public:
  RestrictBackgroundFetchPolicyBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kRestrictBackgroundFetchFromServiceWorker);
  }
  ~RestrictBackgroundFetchPolicyBrowserTest() override = default;

  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    switch (info.param) {
      case Policy::kDefault:
        return "Default";
      case Policy::kTrue:
        return "True";
      case Policy::kFalse:
        return "False";
    }
  }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();

    if (GetParam() == Policy::kDefault) {
      return;
    }
    PolicyMap policies;
    SetPolicy(&policies, key::kRestrictBackgroundFetchFromServiceWorkerEnabled,
              base::Value(GetParam() == Policy::kTrue));
    UpdateProviderPolicy(policies);
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(embedded_test_server()->Start());

    // Allow automatic downloads so background fetch can run to completion if
    // allowed.
    auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
        chrome_test_utils::GetProfile(this));
    ASSERT_TRUE(settings_map);
    ContentSettingsPattern host_pattern =
        ContentSettingsPattern::FromURL(embedded_test_server()->base_url());
    settings_map->SetContentSettingCustomScope(
        host_pattern, ContentSettingsPattern::Wildcard(),
        ContentSettingsType::AUTOMATIC_DOWNLOADS, CONTENT_SETTING_ALLOW);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(RestrictBackgroundFetchPolicyBrowserTest,
                       PolicyIsFollowed) {
  const GURL url =
      embedded_test_server()->GetURL("/background_fetch/background_fetch.html");
  ASSERT_TRUE(NavigateToUrl(url, this));

  // Register the Service Worker.
  ASSERT_EQ("ok - service worker registered",
            EvalJs(chrome_test_utils::GetActiveWebContents(this),
                   "RegisterServiceWorker()"));

  if (GetParam() == Policy::kFalse) {
    // If policy is set to false, the restriction is bypassed/allowed.
    ASSERT_EQ("resolved", EvalJs(chrome_test_utils::GetActiveWebContents(this),
                                 "StartFetchFromServiceWorkerResolve()"));
  } else {
    // By default or if policy is set to true, the restriction is active.
    ASSERT_EQ("NotAllowedError",
              EvalJs(chrome_test_utils::GetActiveWebContents(this),
                     "StartFetchFromServiceWorkerResolve()"));
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    RestrictBackgroundFetchPolicyBrowserTest,
    ::testing::Values(Policy::kDefault, Policy::kTrue, Policy::kFalse),
    &RestrictBackgroundFetchPolicyBrowserTest::DescribeParams);

}  // namespace policy

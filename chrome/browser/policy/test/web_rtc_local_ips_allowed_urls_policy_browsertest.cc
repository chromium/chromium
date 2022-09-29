// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"

#include "base/containers/contains.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Sets the proper policy before the browser is started.
class WebRtcLocalIpsAllowedUrlsTest : public PolicyTest,
                                      public testing::WithParamInterface<int> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kWebRtcLocalIpsAllowedUrls, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(GenerateUrlList()), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  base::Value::List GenerateUrlList() {
    int num_urls = GetParam();
    base::Value::List ret;
    for (int i = 0; i < num_urls; ++i)
      ret.Append(base::NumberToString(i) + ".example.com");

    return ret;
  }
};

IN_PROC_BROWSER_TEST_P(WebRtcLocalIpsAllowedUrlsTest, RunTest) {
  const PrefService::Preference* pref =
      user_prefs::UserPrefs::Get(browser()->profile())
          ->FindPreference(prefs::kWebRtcLocalIpsAllowedUrls);
  EXPECT_TRUE(pref->IsManaged());
  const base::Value::List& allowed_urls = pref->GetValue()->GetList();
  const auto& expected_urls = GenerateUrlList();
  EXPECT_EQ(expected_urls.size(), allowed_urls.size());
  for (const auto& allowed_url : allowed_urls) {
    EXPECT_TRUE(base::Contains(expected_urls, allowed_url));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebRtcLocalIpsAllowedUrlsTest,
                         ::testing::Range(0, 3));

}  // namespace policy

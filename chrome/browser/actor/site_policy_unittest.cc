// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/site_policy.h"

#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/ui/tabs/test/mock_tab_interface.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {

namespace {

class ActorSitePolicyTest : public ChromeRenderViewHostTestHarness {
 public:
  ActorSitePolicyTest() = default;
  ~ActorSitePolicyTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kGlicActionAllowlist, CreateFieldTrialParams());

    ChromeRenderViewHostTestHarness::SetUp();
  }

  virtual base::FieldTrialParams CreateFieldTrialParams() {
    base::FieldTrialParams params;
    params["allowlist"] = "a.test,b.test";
    params["allowlist_only"] = "false";
    return params;
  }

 protected:
  void CheckUrl(const GURL& url, bool expected_allowed) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               url);

    tabs::MockTabInterface tab;
    ON_CALL(tab, GetContents).WillByDefault(::testing::Return(web_contents()));

    base::test::TestFuture<bool> allowed;
    MayActOnTab(tab, allowed.GetCallback());
    // The result should not be provided synchronously.
    EXPECT_FALSE(allowed.IsReady());
    EXPECT_EQ(expected_allowed, allowed.Get());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ActorSitePolicyAllowlistOnlyTest : public ActorSitePolicyTest {
 public:
  ~ActorSitePolicyAllowlistOnlyTest() override = default;

  base::FieldTrialParams CreateFieldTrialParams() override {
    base::FieldTrialParams params;
    params["allowlist"] = "a.test,b.test";
    params["allowlist_exact"] = "exact.test";
    params["allowlist_only"] = "true";
    return params;
  }
};

TEST_F(ActorSitePolicyTest, AllowLocalhost) {
  CheckUrl(GURL("http://localhost/"), true);
  CheckUrl(GURL("http://127.0.0.1/"), true);
  CheckUrl(GURL("http://[::1]/"), true);
}

TEST_F(ActorSitePolicyTest, BlockIpAddress) {
  CheckUrl(GURL("https://8.8.8.8/"), false);
  CheckUrl(GURL("https://[2001:4860:4860::8888]/"), false);
}

TEST_F(ActorSitePolicyTest, BlockNonHTTPScheme) {
  CheckUrl(GURL("file:///my_file"), false);
  CheckUrl(GURL(chrome::kChromeUIVersionURL), false);
}

TEST_F(ActorSitePolicyTest, BlockInsecureHTTP) {
  CheckUrl(GURL("http://a.test/"), false);
}

TEST_F(ActorSitePolicyTest, AllowAllowlistedHosts) {
  CheckUrl(GURL("https://a.test/"), true);
  CheckUrl(GURL("https://b.test/"), true);
}

TEST_F(ActorSitePolicyTest, AllowSubdomain) {
  CheckUrl(GURL("https://subdomain.a.test/"), true);
}

TEST_F(ActorSitePolicyTest, AllowIfNotBlocked) {
  CheckUrl(GURL("https://c.test/"), true);
}

TEST_F(ActorSitePolicyAllowlistOnlyTest, BlockIfNotInAllowlist) {
  CheckUrl(GURL("https://c.test/"), false);
}

TEST_F(ActorSitePolicyAllowlistOnlyTest, BlockSubdomainIfNotInExactAllowlist) {
  CheckUrl(GURL("https://subdomain.exact.test/"), false);
  CheckUrl(GURL("https://exact.test/"), true);
}

}  // namespace

}  // namespace actor

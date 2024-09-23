// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_controller.h"
#include "components/media_router/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace policy {

// Sets the proper policy before the browser is started.
template <bool enable>
class MediaRouterPolicyTest : public PolicyTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kEnableMediaRouter, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(enable),
                 nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

using MediaRouterEnabledPolicyTest = MediaRouterPolicyTest<true>;
using MediaRouterDisabledPolicyTest = MediaRouterPolicyTest<false>;

IN_PROC_BROWSER_TEST_F(MediaRouterEnabledPolicyTest, MediaRouterEnabled) {
  EXPECT_TRUE(media_router::MediaRouterEnabled(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(MediaRouterDisabledPolicyTest, MediaRouterDisabled) {
  EXPECT_FALSE(media_router::MediaRouterEnabled(browser()->profile()));
}

template <bool enable>
class MediaRouterActionPolicyTest : public PolicyTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kShowCastIconInToolbar, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(enable),
                 nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

using MediaRouterActionEnabledPolicyTest = MediaRouterActionPolicyTest<true>;
using MediaRouterActionDisabledPolicyTest = MediaRouterActionPolicyTest<false>;

IN_PROC_BROWSER_TEST_F(MediaRouterActionEnabledPolicyTest,
                       MediaRouterActionEnabled) {
  EXPECT_TRUE(
      CastToolbarButtonController::IsActionShownByPolicy(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(MediaRouterActionDisabledPolicyTest,
                       MediaRouterActionDisabled) {
  EXPECT_FALSE(
      CastToolbarButtonController::IsActionShownByPolicy(browser()->profile()));
}

class MediaRouterCastAllowAllIPsPolicyTest
    : public PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kMediaRouterCastAllowAllIPs, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(is_enabled()), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  bool is_enabled() const { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(MediaRouterCastAllowAllIPsPolicyTest, RunTest) {
  PrefService* const pref = g_browser_process->local_state();
  ASSERT_TRUE(pref);
  EXPECT_EQ(is_enabled(),
            pref->GetBoolean(media_router::prefs::kMediaRouterCastAllowAllIPs));
  EXPECT_TRUE(pref->IsManagedPreference(
      media_router::prefs::kMediaRouterCastAllowAllIPs));
  EXPECT_EQ(is_enabled(), media_router::GetCastAllowAllIPsPref(pref));
}

INSTANTIATE_TEST_SUITE_P(MediaRouterCastAllowAllIPsPolicyTestInstance,
                         MediaRouterCastAllowAllIPsPolicyTest,
                         testing::Values(true, false));
}  // namespace policy

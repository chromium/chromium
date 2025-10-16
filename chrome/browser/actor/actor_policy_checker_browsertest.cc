// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_policy_checker.h"

#include "base/base_switches.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

int ToInt(glic::prefs::GlicActuationOnWebPolicyState state) {
  return static_cast<int>(state);
}

}  // namespace

class ActorPolicyCheckerBrowserTest : public ActorToolsTest {
 public:
  ActorPolicyCheckerBrowserTest() = default;
  ~ActorPolicyCheckerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::SetupCrossSiteRedirector(embedded_test_server());
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void SetUpInProcessBrowserTestFixture() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kNoErrorDialogs);
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void UpdateProviderPolicy(const policy::PolicyMap& policy) {
    policy::PolicyMap policy_with_defaults = policy.Clone();
    policy_provider_.UpdateChromePolicy(policy_with_defaults);
  }

  static base::Value GetActuationOnWebPrefValue(bool enabled) {
    return base::Value(
        enabled ? ToInt(glic::prefs::GlicActuationOnWebPolicyState::kEnabled)
                : ToInt(glic::prefs::GlicActuationOnWebPolicyState::kDisabled));
  }

 private:
  ::testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

class ActorPolicyCheckerBrowserTestAlternatingPolicyValue
    : public ActorPolicyCheckerBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ActorPolicyCheckerBrowserTestAlternatingPolicyValue() = default;
  ~ActorPolicyCheckerBrowserTestAlternatingPolicyValue() override = default;

  void SetUpOnMainThread() override {
    // Note we need to set up the policy before calling the base class's
    // `SetUpOnMainThread()`. The base class's `SetUpOnMainThread()` will create
    // a Task. We don't want to change the policy value after the Task is
    // created, because that will cause the task to be cancelled with
    // `ActionResultCode::kTaskWentAway`.
    policy::PolicyMap policies;
    policies.Set(policy::key::kGeminiActOnWebSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                 GetActuationOnWebPrefValue(GetParam()), nullptr);
    UpdateProviderPolicy(policies);
    ActorPolicyCheckerBrowserTest::SetUpOnMainThread();
  }

  static std::string DescribeParam(const ::testing::TestParamInfo<bool>& info) {
    return info.param ? "ActuationOnWebEnabled" : "ActuationOnWebDisabled";
  }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ActorPolicyCheckerBrowserTestAlternatingPolicyValue,
    ::testing::Bool(),
    ActorPolicyCheckerBrowserTestAlternatingPolicyValue::DescribeParam);

IN_PROC_BROWSER_TEST_F(ActorPolicyCheckerBrowserTest,
                       TasksDroppedWhenActuationCapabilityIsDisabled) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  ASSERT_EQ(prefs->GetInteger(glic::prefs::kGlicActuationOnWeb),
            ToInt(glic::prefs::GlicActuationOnWebPolicyState::kEnabled));

  GURL url = embedded_test_server()->GetURL("/empty.html");
  std::unique_ptr<ToolRequest> action =
      MakeNavigateRequest(*active_tab(), url.spec());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  actor_task().Pause(/*from_actor=*/true);
  EXPECT_EQ(actor_task().GetState(), ActorTask::State::kPausedByActor);

  policy::PolicyMap policies;
  policies.Set(
      policy::key::kGeminiActOnWebSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
      base::Value(ToInt(glic::prefs::GlicActuationOnWebPolicyState::kDisabled)),
      nullptr);
  UpdateProviderPolicy(policies);

  // Note: because we explicitly paused the task, the result will be
  // `ActionResultCode::kError` instead of `ActionResultCode::kTaskWentAway`.
  // See `ActorTask::OnFinishedAct` for more details.
  ExpectErrorResult(result, mojom::ActionResultCode::kError);
}

// Exercise `MayActOnUrl`, which is called by the `ActorNavigationThrottle`.
IN_PROC_BROWSER_TEST_P(ActorPolicyCheckerBrowserTestAlternatingPolicyValue,
                       NavigateOnTab) {
  const bool has_actuation_capability = GetParam();
  ASSERT_EQ(ActorKeyedService::Get(browser()->profile())
                ->GetPolicyChecker()
                .has_actuation_capability(),
            has_actuation_capability);

  // Redirect to a cross-origin URL.
  GURL redirect =
      embedded_test_server()->GetURL("/cross-site/b.com/empty.html");

  std::unique_ptr<ToolRequest> action =
      MakeNavigateRequest(*active_tab(), redirect.spec());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  auto expected_result =
      has_actuation_capability
          ? mojom::ActionResultCode::kOk
          : mojom::ActionResultCode::kTriggeredNavigationBlocked;
  ExpectErrorResult(result, expected_result);
}

// Exercise `MayActOnTab`, which is called by the `ExecutionEngine::Act`.
IN_PROC_BROWSER_TEST_P(ActorPolicyCheckerBrowserTestAlternatingPolicyValue,
                       ActOnTab) {
  const bool has_actuation_capability = GetParam();
  ASSERT_EQ(ActorKeyedService::Get(browser()->profile())
                ->GetPolicyChecker()
                .has_actuation_capability(),
            has_actuation_capability);

  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  int scroll_offset_x = 50;

  std::unique_ptr<ToolRequest> action =
      MakeScrollRequest(*main_frame(),
                        /*content_node_id=*/std::nullopt, scroll_offset_x,
                        /*scroll_offset_y=*/0);
  auto expected_result = has_actuation_capability
                             ? mojom::ActionResultCode::kOk
                             : mojom::ActionResultCode::kUrlBlocked;
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, expected_result);
  EXPECT_EQ(has_actuation_capability ? scroll_offset_x : 0,
            EvalJs(web_contents(), "window.scrollX"));
}

}  // namespace actor

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_policy_checker.h"

#include "base/base_switches.h"
#include "base/test/test_future.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/version.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
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
  return base::to_underlying(state);
}

base::Value GetActuationOnWebPrefValue(bool enabled) {
  return base::Value(
      enabled ? ToInt(glic::prefs::GlicActuationOnWebPolicyState::kEnabled)
              : ToInt(glic::prefs::GlicActuationOnWebPolicyState::kDisabled));
}

}  // namespace

class ActorPolicyCheckerBrowserTestBase : public ActorToolsTest {
 public:
  ActorPolicyCheckerBrowserTestBase() = default;
  ~ActorPolicyCheckerBrowserTestBase() override = default;

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

 private:
  ::testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

// Tests that exercise the policy checker for non-managed clients.
class ActorPolicyCheckerBrowserTestNonManaged
    : public ActorPolicyCheckerBrowserTestBase {
 public:
  ActorPolicyCheckerBrowserTestNonManaged() = default;
  ~ActorPolicyCheckerBrowserTestNonManaged() override = default;

  void SetUpOnMainThread() override {
    ActorPolicyCheckerBrowserTestBase::SetUpOnMainThread();
    auto* management_service_factory =
        policy::ManagementServiceFactory::GetInstance();
    auto* browser_management_service =
        management_service_factory->GetForProfile(GetProfile());
    ASSERT_TRUE(!browser_management_service ||
                !browser_management_service->IsManaged());
  }
};

// Tests that exercise the policy checker for managed clients.
class ActorPolicyCheckerBrowserTestManaged
    : public ActorPolicyCheckerBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  ActorPolicyCheckerBrowserTestManaged() = default;
  ~ActorPolicyCheckerBrowserTestManaged() override = default;

  void SetUpOnMainThread() override {
    const base::Version version = version_info::GetVersion();
    bool enable_policy = GetParam();
    // TODO(crbug.com/452629096): Remove this condition once we can enable the
    // capability via the policy, after M145.
    if (version.IsValid() && version < base::Version("145") && enable_policy) {
      GTEST_SKIP() << "Between now to M145, act-on-web capability is strictly "
                      "disabled on managed clients, and not even controllable "
                      "via the policy. Skip those tests where we want to "
                      "enable the policy.";
    }
    // Set up the policy so the profile becomes managed.
    //
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
    ActorPolicyCheckerBrowserTestBase::SetUpOnMainThread();

    auto* management_service_factory =
        policy::ManagementServiceFactory::GetInstance();
    auto* browser_management_service =
        management_service_factory->GetForProfile(GetProfile());
    ASSERT_TRUE(browser_management_service);
    ASSERT_TRUE(browser_management_service->IsManaged());
  }

  static std::string DescribeParam(const ::testing::TestParamInfo<bool>& info) {
    return info.param ? "ActuationOnWebEnabled" : "ActuationOnWebDisabled";
  }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ActorPolicyCheckerBrowserTestManaged,
    ::testing::Bool(),
    ActorPolicyCheckerBrowserTestManaged::DescribeParam);

IN_PROC_BROWSER_TEST_F(ActorPolicyCheckerBrowserTestNonManaged,
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

  // Since the profile is not managed, we can change the capability by changing
  // the pref.
  prefs->SetInteger(
      glic::prefs::kGlicActuationOnWeb,
      ToInt(glic::prefs::GlicActuationOnWebPolicyState::kDisabled));
  EXPECT_EQ(prefs->GetInteger(glic::prefs::kGlicActuationOnWeb),
            ToInt(glic::prefs::GlicActuationOnWebPolicyState::kDisabled));

  // Note: because we explicitly paused the task, the result will be
  // `ActionResultCode::kError` instead of `ActionResultCode::kTaskWentAway`.
  // See `ActorTask::OnFinishedAct` for more details.
  ExpectErrorResult(result, mojom::ActionResultCode::kError);
}

// Exercise `MayActOnUrl`, which is called by the `ActorNavigationThrottle`.
IN_PROC_BROWSER_TEST_P(ActorPolicyCheckerBrowserTestManaged, NavigateOnTab) {
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
IN_PROC_BROWSER_TEST_P(ActorPolicyCheckerBrowserTestManaged, ActOnTab) {
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

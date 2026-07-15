// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/site_policy.h"

#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/test_support/mock_event_dispatcher.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/actor/core/actor_features.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/origin_gating/core/origin_gating_cache.h"
#include "components/origin_gating/core/origin_gating_checker.h"
#include "components/tabs/public/mock_tab_interface.h"
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
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /* enabled_features = */ {{features::kGlicActor, {}},
                                  {kGlicActionAllowlist,
                                   CreateFieldTrialParams()}},
        /* disabled_features = */ {});

    ChromeRenderViewHostTestHarness::SetUp();

    mock_optimization_guide_keyed_service_ =
        static_cast<MockOptimizationGuideKeyedService*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile(),
                    base::BindOnce(
                        &ActorSitePolicyTest::CreateOptimizationService)));

    ON_CALL(*mock_optimization_guide_keyed_service_,
            CanApplyOptimization(
                testing::_, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
                testing::An<
                    optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillByDefault(base::test::RunOnceCallback<2>(
            optimization_guide::OptimizationGuideDecision::kTrue,
            optimization_guide::OptimizationMetadata{}));

    // Simulate the component loading, as the implementation checks it, but the
    // actual outcomes are determined by the mock.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
        ->MaybeUpdateHintsComponent(
            {base::Version("123"),
             temp_dir_.GetPath().Append(FILE_PATH_LITERAL("dont_care"))});
  }

  void TearDown() override {
    if (task_) {
      if (!task_->IsCompleted()) {
        task_->Stop(ActorTask::StoppedReason::kTabDetached);
      }
      task_.reset();
    }
    mock_optimization_guide_keyed_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  virtual base::FieldTrialParams CreateFieldTrialParams() {
    base::FieldTrialParams params;
    params["allowlist"] = "a.test,b.test";
    params["allowlist_only"] = "false";
    return params;
  }

 protected:
  void SetExpectedOptimizationGuideCall(
      const GURL& url,
      optimization_guide::OptimizationGuideDecision result) {
    EXPECT_CALL(
        *mock_optimization_guide_keyed_service_,
        CanApplyOptimization(
            url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
            testing::An<
                optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillOnce(base::test::RunOnceCallback<2>(
            result, optimization_guide::OptimizationMetadata{}));
  }

  void CheckUrl(const GURL& url,
                bool expected_allowed,
                EnterprisePolicyChecker::UrlBlockReason enterprise_reason) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               url);

    tabs::MockTabInterface tab;
    ON_CALL(tab, GetContents).WillByDefault(::testing::Return(web_contents()));

    auto* actor_service = ActorKeyedService::Get(profile());
    // The enterprise-policy check runs inside the OriginGatingChecker, which
    // reads the task's policy checker via the delegate. Configure that checker
    // rather than passing one to MayActOnTab.
    task_policy_checker_.set_reason(enterprise_reason);
    base::test::TestFuture<MayActOnUrlBlockReason> allowed;
    GetExecutionEngine().MayActOnTab(tab, actor_service->GetJournal(), TaskId(),
                                     allowed.GetCallback());
    // The result should not be provided synchronously.
    EXPECT_FALSE(allowed.IsReady());
    EXPECT_EQ(expected_allowed,
              allowed.Get() == MayActOnUrlBlockReason::kAllowed);
  }

  void CheckUrl(const GURL& url, bool expected_allowed) {
    return CheckUrl(url, expected_allowed,
                    EnterprisePolicyChecker::UrlBlockReason::kNotBlocked);
  }

  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;

  // Lazily creates a real ActorTask/ExecutionEngine and returns its engine, so
  // that MayActOnUrl tests can exercise the OriginGatingChecker-backed path.
  ExecutionEngine& GetExecutionEngine() {
    if (!task_) {
      std::unique_ptr<ui::UiEventDispatcher> engine_dispatcher =
          ui::NewMockUiEventDispatcher();
      std::unique_ptr<ui::UiEventDispatcher> task_dispatcher =
          ui::NewMockUiEventDispatcher();
      ScopedExecutionEngineFactory scoped_factory(base::BindLambdaForTesting(
          [&](ActorTask& task) -> std::unique_ptr<ExecutionEngine> {
            return ExecutionEngine::CreateForTesting(
                task, std::move(engine_dispatcher));
          }));
      task_ = ActorTask::CreateForTesting(
          *ActorKeyedService::Get(profile()), TaskId(1),
          std::move(task_dispatcher),
          /*options=*/nullptr, TestTaskSourceInfo(), &task_policy_checker_,
          mock_actor_task_delegate_.GetWeakPtr());
    }
    return task_->GetExecutionEngine();
  }

 private:
  static std::unique_ptr<KeyedService> CreateOptimizationService(
      content::BrowserContext* context) {
    return std::make_unique<MockOptimizationGuideKeyedService>();
  }

  std::unique_ptr<ActorTask> task_;
  MockPolicyChecker task_policy_checker_{
      EnterprisePolicyChecker::UrlBlockReason::kNotBlocked};
  testing::NiceMock<MockActorTaskDelegate> mock_actor_task_delegate_;

  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
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

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_AllowLocalhost DISABLED_AllowLocalhost
#else
#define MAYBE_AllowLocalhost AllowLocalhost
#endif
TEST_F(ActorSitePolicyTest, MAYBE_AllowLocalhost) {
  CheckUrl(GURL("http://localhost/"), true);
  CheckUrl(GURL("http://127.0.0.1/"), true);
  CheckUrl(GURL("http://[::1]/"), true);
}

TEST_F(ActorSitePolicyTest, AllowAboutBlank) {
  CheckUrl(GURL(url::kAboutBlankURL), true);
}

// TODO(crbug.com/480230075): Crashing on Android.
#if BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#define MAYBE_BlockIpAddress DISABLED_BlockIpAddress
#else
#define MAYBE_BlockIpAddress BlockIpAddress
#endif
TEST_F(ActorSitePolicyTest, MAYBE_BlockIpAddress) {
  CheckUrl(GURL("https://8.8.8.8/"), false);
  CheckUrl(GURL("https://[2001:4860:4860::8888]/"), false);
}

TEST_F(ActorSitePolicyTest, BlockNonHTTPScheme) {
  CheckUrl(GURL("file:///my_file"), false);
  CheckUrl(GURL("file://localhost/tmp"), false);
  CheckUrl(GURL(chrome::kChromeUIVersionURL), false);
}

TEST_F(ActorSitePolicyTest, BlockInsecureHTTP) {
  CheckUrl(GURL("http://a.test/"), false);
}

TEST_F(ActorSitePolicyTest, InsecureHTTPAllowedWhenSpecified) {
  base::test::TestFuture<MayActOnUrlBlockReason> allowed;
  GetExecutionEngine().IsAcceptableNavigationDestination(GURL("http://a.test/"),
                                                         allowed.GetCallback());
  EXPECT_EQ(allowed.Get(), MayActOnUrlBlockReason::kAllowed);
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

TEST_F(ActorSitePolicyTest, BlockIfInBlocklist) {
  const GURL url("https://c.test/");
  SetExpectedOptimizationGuideCall(
      url, optimization_guide::OptimizationGuideDecision::kFalse);
  CheckUrl(url, false);
}

TEST_F(ActorSitePolicyTest, AllowIfNotBlockedForOriginGating) {
  base::test::TestFuture<bool> got_may_act;
  EXPECT_THAT(
      MaybeCheckOptimizationGuideForSensitiveUrl(
          GURL("https://c.test/"), profile(), got_may_act.GetCallback()),
      base::test::HasValue());
  EXPECT_TRUE(got_may_act.Get());
}

TEST_F(ActorSitePolicyTest, BlockIfInBlocklistForOriginGating) {
  const GURL url("https://c.test/");
  SetExpectedOptimizationGuideCall(
      url, optimization_guide::OptimizationGuideDecision::kFalse);
  base::test::TestFuture<bool> got_may_act;
  EXPECT_THAT(MaybeCheckOptimizationGuideForSensitiveUrl(
                  url, profile(), got_may_act.GetCallback()),
              base::test::HasValue());
  EXPECT_FALSE(got_may_act.Get());
}

TEST_F(ActorSitePolicyTest, AllowedOriginsFromNavigationGating) {
  const GURL url("https://c.test/");

  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(2)
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kFalse,
          optimization_guide::OptimizationMetadata{}));

  {
    CheckUrl(url, false);
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kGlicActionAllowlist, CreateFieldTrialParams()},
         {kGlicCrossOriginNavigationGating, base::FieldTrialParams()}},
        {});
    CheckUrl(url, true);
  }
}

TEST_F(ActorSitePolicyTest, AllowIfDecisionUnknown) {
  const GURL url("https://c.test/");
  SetExpectedOptimizationGuideCall(
      url, optimization_guide::OptimizationGuideDecision::kUnknown);
  CheckUrl(url, true);
}

TEST_F(ActorSitePolicyTest, EnterprisePolicyBlock) {
  const GURL url("https://c.test/");
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);
  CheckUrl(url, false,
           EnterprisePolicyChecker::UrlBlockReason::kExplicitlyBlocked);
}

TEST_F(ActorSitePolicyTest, EnterprisePolicyOrder) {
  const GURL https_blocked_url("https://c.test/");
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          https_blocked_url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);
  // Enterprise policy overrules the opt guide blocklist for a particular site.
  CheckUrl(https_blocked_url, true,
           EnterprisePolicyChecker::UrlBlockReason::kExplicitlyAllowed);
  // Enterprise policy can't be used to bypass invariants like supported
  // schemes.
  CheckUrl(GURL("file:///my_file"), false,
           EnterprisePolicyChecker::UrlBlockReason::kExplicitlyAllowed);
}

TEST_F(ActorSitePolicyAllowlistOnlyTest, BlockIfNotInAllowlist) {
  CheckUrl(GURL("https://c.test/"), false);
}

TEST_F(ActorSitePolicyAllowlistOnlyTest, BlockSubdomainIfNotInExactAllowlist) {
  CheckUrl(GURL("https://subdomain.exact.test/"), false);
  CheckUrl(GURL("https://exact.test/"), true);
}

TEST_F(ActorSitePolicyTest, MayActOnUrl_AllowedByCache) {
  const GURL url("https://c.test/");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kGlicActionAllowlist, CreateFieldTrialParams()},
       {kGlicCrossOriginNavigationGating, {}}},
      {});

  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);

  ExecutionEngine& engine = GetExecutionEngine();
  engine.origin_gating_checker().AllowNavigationTo(url::Origin::Create(url),
                                                   /*is_user_confirmed=*/true);
  base::test::TestFuture<MayActOnUrlBlockReason> allowed;
  engine.IsAcceptableNavigationDestination(url, allowed.GetCallback());
  // Allowed by cache.
  EXPECT_EQ(allowed.Get(), MayActOnUrlBlockReason::kAllowed);
}

TEST_F(ActorSitePolicyTest, MayActOnUrl_FailsOpen) {
  const GURL url("https://c.test/");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kGlicActionAllowlist, CreateFieldTrialParams()},
       {kGlicCrossOriginNavigationGating, {}}},
      {});

  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);

  base::test::TestFuture<MayActOnUrlBlockReason> allowed;
  GetExecutionEngine().IsAcceptableNavigationDestination(url,
                                                         allowed.GetCallback());
  // Not allowed by the cache, but the policy fails open (without consulting
  // the sensitive sites list).
  EXPECT_EQ(allowed.Get(), MayActOnUrlBlockReason::kAllowed);
}

TEST_F(ActorSitePolicyTest, MayActOnTab_AllowedByCache) {
  const GURL url("https://c.test/");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kGlicActionAllowlist, CreateFieldTrialParams()},
       {kGlicCrossOriginNavigationGating, {}}},
      {});

  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(
          url, optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);

  GetExecutionEngine().origin_gating_checker().AllowNavigationTo(
      url::Origin::Create(url), /*is_user_confirmed=*/true);
  CheckUrl(url, /*expected_allowed=*/true);
}

}  // namespace

}  // namespace actor

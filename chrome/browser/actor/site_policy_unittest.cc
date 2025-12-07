// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/site_policy.h"

#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
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
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kGlicActionAllowlist, CreateFieldTrialParams());

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
                absl::flat_hash_set<url::Origin> allowed_origins =
                    absl::flat_hash_set<url::Origin>()) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               url);

    tabs::MockTabInterface tab;
    ON_CALL(tab, GetContents).WillByDefault(::testing::Return(web_contents()));

    auto* actor_service = ActorKeyedService::Get(profile());
    base::test::TestFuture<MayActOnUrlBlockReason> allowed;
    MayActOnTab(tab, actor_service->GetJournal(), TaskId(),
                absl::flat_hash_set<url::Origin>(), allowed.GetCallback());
    // The result should not be provided synchronously.
    EXPECT_FALSE(allowed.IsReady());
    EXPECT_EQ(expected_allowed,
              allowed.Get() == MayActOnUrlBlockReason::kAllowed);
  }

  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;

 private:
  static std::unique_ptr<KeyedService> CreateOptimizationService(
      content::BrowserContext* context) {
    return std::make_unique<MockOptimizationGuideKeyedService>();
  }

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

TEST_F(ActorSitePolicyTest, AllowLocalhost) {
  CheckUrl(GURL("http://localhost/"), true);
  CheckUrl(GURL("http://127.0.0.1/"), true);
  CheckUrl(GURL("http://[::1]/"), true);
}

TEST_F(ActorSitePolicyTest, AllowAboutBlank) {
  CheckUrl(GURL(url::kAboutBlankURL), true);
}

TEST_F(ActorSitePolicyTest, BlockIpAddress) {
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
  base::test::TestFuture<bool> allowed;
  MayActOnUrl(GURL("http://a.test/"), /*allow_insecure_http=*/true, profile(),
              ActorKeyedService::Get(profile())->GetJournal(), TaskId(),
              allowed.GetCallback());
  EXPECT_TRUE(allowed.Get());
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
  EXPECT_TRUE(ShouldBlockNavigationUrlForOriginGating(
      GURL("https://c.test/"), profile(), got_may_act.GetCallback()));
  EXPECT_TRUE(got_may_act.Get());
}

TEST_F(ActorSitePolicyTest, BlockIfInBlocklistForOriginGating) {
  const GURL url("https://c.test/");
  SetExpectedOptimizationGuideCall(
      url, optimization_guide::OptimizationGuideDecision::kFalse);
  base::test::TestFuture<bool> got_may_act;
  EXPECT_TRUE(ShouldBlockNavigationUrlForOriginGating(
      url, profile(), got_may_act.GetCallback()));
  EXPECT_FALSE(got_may_act.Get());
}

TEST_F(ActorSitePolicyTest, AllowedOriginsFromNavigationGating) {
  const GURL url("https://c.test/");
  const absl::flat_hash_set<url::Origin> allowed_origins = {
      url::Origin::Create(GURL("https://c.test/"))};

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
    CheckUrl(url, false, allowed_origins);
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kGlicActionAllowlist, CreateFieldTrialParams()},
         {kGlicCrossOriginNavigationGating, base::FieldTrialParams()}},
        {});
    CheckUrl(url, true, allowed_origins);
  }
}

TEST_F(ActorSitePolicyTest, AllowIfDecisionUnknown) {
  const GURL url("https://c.test/");
  SetExpectedOptimizationGuideCall(
      url, optimization_guide::OptimizationGuideDecision::kUnknown);
  CheckUrl(url, true);
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

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/actor_login_flow_verifier.h"

#include <memory>
#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/tools/attempt_otp_filling_metrics.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_login_context.h"
#include "chrome/test/base/testing_profile.h"
#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {

class ActorLoginFlowVerifierTest : public testing::Test {
 public:
  ActorLoginFlowVerifierTest() = default;
  ~ActorLoginFlowVerifierTest() override = default;

  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
    web_contents_ = web_contents_factory_.CreateWebContents(profile_.get());
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents_, GURL("https://example.com"));
    verifier_ =
        std::make_unique<ActorLoginFlowVerifier>(fake_affiliation_service_);
  }

  content::RenderFrameHost* main_rfh() {
    return web_contents_->GetPrimaryMainFrame();
  }

 protected:
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  affiliations::FakeAffiliationService fake_affiliation_service_;
  std::unique_ptr<ActorLoginFlowVerifier> verifier_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ActorLoginFlowVerifierTest, NullContext_ReturnsFalse) {
  base::test::TestFuture<bool> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(), main_rfh()->GetLastCommittedOrigin(),
      main_rfh()->GetLastCommittedOrigin(), std::nullopt, future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester_.ExpectBucketCount(kActorOtpVerifyIsActorLoginFlowHistogram,
                                      VerifyIsActorLoginFlowEvent::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOtpVerifyIsActorLoginFlowHistogram,
      VerifyIsActorLoginFlowEvent::kNoActorLoginContext, 1);
}

TEST_F(ActorLoginFlowVerifierTest, FrameNotInContext_ReturnsFalse) {
  autofill::ActorLoginContext context(main_rfh()->GetLastCommittedOrigin(),
                                      /*should_use_strong_matching=*/false,
                                      /*navigations_per_frame=*/{});

  base::test::TestFuture<bool> future;
  verifier_->VerifyIsActorLoginFlow(main_rfh()->GetFrameTreeNodeId(),
                                    main_rfh()->GetLastCommittedOrigin(),
                                    main_rfh()->GetLastCommittedOrigin(),
                                    std::move(context), future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester_.ExpectBucketCount(kActorOtpVerifyIsActorLoginFlowHistogram,
                                      VerifyIsActorLoginFlowEvent::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOtpVerifyIsActorLoginFlowHistogram,
      VerifyIsActorLoginFlowEvent::kFrameNotInLoginContext, 1);
}

TEST_F(ActorLoginFlowVerifierTest, TooManyNavigations_ReturnsFalse) {
  autofill::ActorLoginContext context(
      main_rfh()->GetLastCommittedOrigin(),
      /*should_use_strong_matching=*/false,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 2}});

  base::test::TestFuture<bool> future;
  verifier_->VerifyIsActorLoginFlow(main_rfh()->GetFrameTreeNodeId(),
                                    main_rfh()->GetLastCommittedOrigin(),
                                    main_rfh()->GetLastCommittedOrigin(),
                                    std::move(context), future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester_.ExpectBucketCount(kActorOtpVerifyIsActorLoginFlowHistogram,
                                      VerifyIsActorLoginFlowEvent::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOtpVerifyIsActorLoginFlowHistogram,
      VerifyIsActorLoginFlowEvent::kAllFramesHaveTooManyNavigations, 1);
}

TEST_F(ActorLoginFlowVerifierTest, SameOrigin_ReturnsTrue) {
  autofill::ActorLoginContext context(
      main_rfh()->GetLastCommittedOrigin(),
      /*should_use_strong_matching=*/false,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 1}});

  base::test::TestFuture<bool> future;
  verifier_->VerifyIsActorLoginFlow(main_rfh()->GetFrameTreeNodeId(),
                                    main_rfh()->GetLastCommittedOrigin(),
                                    main_rfh()->GetLastCommittedOrigin(),
                                    std::move(context), future.GetCallback());

  EXPECT_TRUE(future.Get());
  histogram_tester_.ExpectBucketCount(kActorOtpVerifyIsActorLoginFlowHistogram,
                                      VerifyIsActorLoginFlowEvent::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOtpVerifyIsActorLoginFlowHistogram,
      VerifyIsActorLoginFlowEvent::kExactMatchAllowed, 1);
}

TEST_F(ActorLoginFlowVerifierTest, OtpFrameOriginMismatch_ReturnsFalse) {
  autofill::ActorLoginContext context(
      main_rfh()->GetLastCommittedOrigin(),
      /*should_use_strong_matching=*/false,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 1}});

  base::test::TestFuture<bool> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(),
      url::Origin::Create(GURL("https://other-domain.com")),
      main_rfh()->GetLastCommittedOrigin(), std::move(context),
      future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester_.ExpectBucketCount(kActorOtpVerifyIsActorLoginFlowHistogram,
                                      VerifyIsActorLoginFlowEvent::kStart, 1);
  histogram_tester_.ExpectBucketCount(kActorOtpVerifyIsActorLoginFlowHistogram,
                                      VerifyIsActorLoginFlowEvent::kNoMatch, 1);
}

TEST_F(ActorLoginFlowVerifierTest, AffiliatedOrigin_ReturnsTrue) {
  affiliations::AffiliatedFacets group = {
      affiliations::Facet(
          affiliations::FacetURI::FromCanonicalSpec("https://example.com")),
      affiliations::Facet(
          affiliations::FacetURI::FromCanonicalSpec("https://affiliated.com"))};
  fake_affiliation_service_.AddAffiliationGroup(group);
  autofill::ActorLoginContext context(
      url::Origin::Create(GURL("https://affiliated.com")),
      /*should_use_strong_matching=*/true,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 1}});

  base::test::TestFuture<bool> future;
  verifier_->VerifyIsActorLoginFlow(main_rfh()->GetFrameTreeNodeId(),
                                    main_rfh()->GetLastCommittedOrigin(),
                                    main_rfh()->GetLastCommittedOrigin(),
                                    std::move(context), future.GetCallback());

  EXPECT_TRUE(future.Get());
  histogram_tester_.ExpectBucketCount(kActorOtpVerifyIsActorLoginFlowHistogram,
                                      VerifyIsActorLoginFlowEvent::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOtpVerifyIsActorLoginFlowHistogram,
      VerifyIsActorLoginFlowEvent::kAffiliatedMatchAllowed, 1);
}

TEST_F(ActorLoginFlowVerifierTest, PslMatch_WeakMatchingAllowed_ReturnsTrue) {
  autofill::ActorLoginContext context(
      url::Origin::Create(GURL("https://b.example.com")),
      /*should_use_strong_matching=*/false,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 1}});

  base::test::TestFuture<bool> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(),
      url::Origin::Create(GURL("https://a.example.com")),
      url::Origin::Create(GURL("https://b.example.com")), std::move(context),
      future.GetCallback());

  EXPECT_TRUE(future.Get());
  histogram_tester_.ExpectBucketCount(kActorOtpVerifyIsActorLoginFlowHistogram,
                                      VerifyIsActorLoginFlowEvent::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOtpVerifyIsActorLoginFlowHistogram,
      VerifyIsActorLoginFlowEvent::kPslMatchAllowed, 1);
}

TEST_F(ActorLoginFlowVerifierTest,
       PslMatch_StrongMatchingRequired_ReturnsFalse) {
  autofill::ActorLoginContext context(
      url::Origin::Create(GURL("https://b.example.com")),
      /*should_use_strong_matching=*/true,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 1}});

  base::test::TestFuture<bool> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(),
      url::Origin::Create(GURL("https://a.example.com")),
      url::Origin::Create(GURL("https://b.example.com")), std::move(context),
      future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester_.ExpectBucketCount(kActorOtpVerifyIsActorLoginFlowHistogram,
                                      VerifyIsActorLoginFlowEvent::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOtpVerifyIsActorLoginFlowHistogram,
      VerifyIsActorLoginFlowEvent::kPslMatchDisallowed, 1);
}

TEST_F(ActorLoginFlowVerifierTest, GroupedOrigin_ReturnsFalse) {
  affiliations::GroupedFacets group;
  // push_back is preferred over emplace_back: https://abseil.io/tips/112
  group.facets.push_back(affiliations::Facet(
      affiliations::FacetURI::FromCanonicalSpec("https://example.com")));
  group.facets.push_back(affiliations::Facet(
      affiliations::FacetURI::FromCanonicalSpec("https://grouped.com")));
  fake_affiliation_service_.AddGroupedFacets(group);
  autofill::ActorLoginContext context(
      main_rfh()->GetLastCommittedOrigin(),
      /*should_use_strong_matching=*/false,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 1}});

  base::test::TestFuture<bool> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(),
      url::Origin::Create(GURL("https://grouped.com")),
      main_rfh()->GetLastCommittedOrigin(), std::move(context),
      future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester_.ExpectBucketCount(kActorOtpVerifyIsActorLoginFlowHistogram,
                                      VerifyIsActorLoginFlowEvent::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOtpVerifyIsActorLoginFlowHistogram,
      VerifyIsActorLoginFlowEvent::kGroupedOrOtherMismatch, 1);
}

TEST_F(ActorLoginFlowVerifierTest, MainFramePslMatch_ReturnsFalse) {
  autofill::ActorLoginContext context(
      url::Origin::Create(GURL("https://b.example.com")),
      /*should_use_strong_matching=*/false,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 1}});

  base::test::TestFuture<bool> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(),
      url::Origin::Create(GURL("https://b.example.com")),
      url::Origin::Create(GURL("https://a.example.com")), std::move(context),
      future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester_.ExpectBucketCount(kActorOtpVerifyIsActorLoginFlowHistogram,
                                      VerifyIsActorLoginFlowEvent::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOtpVerifyIsActorLoginFlowHistogram,
      VerifyIsActorLoginFlowEvent::kMainFrameOriginMismatch, 1);
}

TEST_F(ActorLoginFlowVerifierTest, MainFrameGroupedOrigin_ReturnsFalse) {
  affiliations::GroupedFacets group;
  group.facets.emplace_back(
      affiliations::FacetURI::FromCanonicalSpec("https://example.com"));
  group.facets.emplace_back(
      affiliations::FacetURI::FromCanonicalSpec("https://grouped.com"));
  fake_affiliation_service_.AddGroupedFacets(group);
  autofill::ActorLoginContext context(
      url::Origin::Create(GURL("https://grouped.com")),
      /*should_use_strong_matching=*/false,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 1}});

  base::test::TestFuture<bool> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(),
      url::Origin::Create(GURL("https://grouped.com")),
      main_rfh()->GetLastCommittedOrigin(), std::move(context),
      future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester_.ExpectBucketCount(kActorOtpVerifyIsActorLoginFlowHistogram,
                                      VerifyIsActorLoginFlowEvent::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOtpVerifyIsActorLoginFlowHistogram,
      VerifyIsActorLoginFlowEvent::kMainFrameOriginMismatch, 1);
}

}  // namespace actor

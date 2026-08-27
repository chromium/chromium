// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/actor_login_flow_verifier.h"

#include <memory>
#include <optional>

#include "base/strings/to_string.h"
#include "base/test/test_future.h"
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

  base::OnceCallback<std::optional<autofill::ActorLoginContext>()>
  MakeConsumeContextCallback(
      std::optional<autofill::ActorLoginContext> context) {
    return base::BindOnce(
        [](std::optional<autofill::ActorLoginContext> ctx) { return ctx; },
        std::move(context));
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
};

TEST_F(ActorLoginFlowVerifierTest, NullContext_ReturnsNoActorLoginContext) {
  base::test::TestFuture<ActorLoginFlowVerifier::Result> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(), main_rfh()->GetLastCommittedOrigin(),
      main_rfh()->GetLastCommittedOrigin(), std::nullopt,
      /*should_use_strong_matching=*/false,
      MakeConsumeContextCallback(std::nullopt), future.GetCallback());

  EXPECT_EQ(future.Get(), ActorLoginFlowVerifier::Result::kNoActorLoginContext);
  EXPECT_FALSE(ActorLoginFlowVerifier::IsSuccess(future.Get()));
}

TEST_F(ActorLoginFlowVerifierTest,
       NullContextAfterCheck_ReturnsNoActorLoginContext) {
  base::test::TestFuture<ActorLoginFlowVerifier::Result> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(), main_rfh()->GetLastCommittedOrigin(),
      main_rfh()->GetLastCommittedOrigin(),
      main_rfh()->GetLastCommittedOrigin(),
      /*should_use_strong_matching=*/false,
      MakeConsumeContextCallback(std::nullopt), future.GetCallback());

  EXPECT_EQ(future.Get(), ActorLoginFlowVerifier::Result::kNoActorLoginContext);
  EXPECT_FALSE(ActorLoginFlowVerifier::IsSuccess(future.Get()));
}

TEST_F(ActorLoginFlowVerifierTest,
       FrameNotInContext_ReturnsFrameNotInLoginContext) {
  autofill::ActorLoginContext context(main_rfh()->GetLastCommittedOrigin(),
                                      /*should_use_strong_matching=*/false,
                                      /*navigations_per_frame=*/{});
  url::Origin context_origin = context.origin;
  bool should_use_strong_matching = context.should_use_strong_matching;

  base::test::TestFuture<ActorLoginFlowVerifier::Result> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(), main_rfh()->GetLastCommittedOrigin(),
      main_rfh()->GetLastCommittedOrigin(), context_origin,
      should_use_strong_matching,
      MakeConsumeContextCallback(std::move(context)), future.GetCallback());

  EXPECT_EQ(future.Get(),
            ActorLoginFlowVerifier::Result::kFrameNotInLoginContext);
  EXPECT_FALSE(ActorLoginFlowVerifier::IsSuccess(future.Get()));
}

TEST_F(ActorLoginFlowVerifierTest,
       TooManyNavigations_ReturnsAllFramesHaveTooManyNavigations) {
  autofill::ActorLoginContext context(
      main_rfh()->GetLastCommittedOrigin(),
      /*should_use_strong_matching=*/false,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 2}});
  url::Origin context_origin = context.origin;
  bool should_use_strong_matching = context.should_use_strong_matching;

  base::test::TestFuture<ActorLoginFlowVerifier::Result> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(), main_rfh()->GetLastCommittedOrigin(),
      main_rfh()->GetLastCommittedOrigin(), context_origin,
      should_use_strong_matching,
      MakeConsumeContextCallback(std::move(context)), future.GetCallback());

  EXPECT_EQ(future.Get(),
            ActorLoginFlowVerifier::Result::kAllFramesHaveTooManyNavigations);
  EXPECT_FALSE(ActorLoginFlowVerifier::IsSuccess(future.Get()));
}

TEST_F(ActorLoginFlowVerifierTest, SameOrigin_ReturnsExactMatchAllowed) {
  autofill::ActorLoginContext context(
      main_rfh()->GetLastCommittedOrigin(),
      /*should_use_strong_matching=*/false,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 1}});
  url::Origin context_origin = context.origin;
  bool should_use_strong_matching = context.should_use_strong_matching;

  base::test::TestFuture<ActorLoginFlowVerifier::Result> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(), main_rfh()->GetLastCommittedOrigin(),
      main_rfh()->GetLastCommittedOrigin(), context_origin,
      should_use_strong_matching,
      MakeConsumeContextCallback(std::move(context)), future.GetCallback());

  EXPECT_EQ(future.Get(), ActorLoginFlowVerifier::Result::kExactMatchAllowed);
  EXPECT_TRUE(ActorLoginFlowVerifier::IsSuccess(future.Get()));
}

TEST_F(ActorLoginFlowVerifierTest, OtpFrameOriginMismatch_ReturnsNoMatch) {
  autofill::ActorLoginContext context(
      main_rfh()->GetLastCommittedOrigin(),
      /*should_use_strong_matching=*/false,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 1}});
  url::Origin context_origin = context.origin;
  bool should_use_strong_matching = context.should_use_strong_matching;

  base::test::TestFuture<ActorLoginFlowVerifier::Result> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(),
      url::Origin::Create(GURL("https://other-domain.com")),
      main_rfh()->GetLastCommittedOrigin(), context_origin,
      should_use_strong_matching,
      MakeConsumeContextCallback(std::move(context)), future.GetCallback());

  EXPECT_EQ(future.Get(), ActorLoginFlowVerifier::Result::kNoMatch);
  EXPECT_FALSE(ActorLoginFlowVerifier::IsSuccess(future.Get()));
}

TEST_F(ActorLoginFlowVerifierTest,
       AffiliatedOrigin_ReturnsAffiliatedMatchAllowed) {
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
  url::Origin context_origin = context.origin;
  bool should_use_strong_matching = context.should_use_strong_matching;

  base::test::TestFuture<ActorLoginFlowVerifier::Result> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(), main_rfh()->GetLastCommittedOrigin(),
      main_rfh()->GetLastCommittedOrigin(), context_origin,
      should_use_strong_matching,
      MakeConsumeContextCallback(std::move(context)), future.GetCallback());

  EXPECT_EQ(future.Get(),
            ActorLoginFlowVerifier::Result::kAffiliatedMatchAllowed);
  EXPECT_TRUE(ActorLoginFlowVerifier::IsSuccess(future.Get()));
}

TEST_F(ActorLoginFlowVerifierTest,
       PslMatch_WeakMatchingAllowed_ReturnsPslMatchAllowed) {
  autofill::ActorLoginContext context(
      url::Origin::Create(GURL("https://b.example.com")),
      /*should_use_strong_matching=*/false,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 1}});
  url::Origin context_origin = context.origin;
  bool should_use_strong_matching = context.should_use_strong_matching;

  base::test::TestFuture<ActorLoginFlowVerifier::Result> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(),
      url::Origin::Create(GURL("https://a.example.com")),
      url::Origin::Create(GURL("https://b.example.com")), context_origin,
      should_use_strong_matching,
      MakeConsumeContextCallback(std::move(context)), future.GetCallback());

  EXPECT_EQ(future.Get(), ActorLoginFlowVerifier::Result::kPslMatchAllowed);
  EXPECT_TRUE(ActorLoginFlowVerifier::IsSuccess(future.Get()));
}

TEST_F(ActorLoginFlowVerifierTest,
       PslMatch_StrongMatchingRequired_ReturnsPslMatchDisallowed) {
  autofill::ActorLoginContext context(
      url::Origin::Create(GURL("https://b.example.com")),
      /*should_use_strong_matching=*/true,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 1}});
  url::Origin context_origin = context.origin;
  bool should_use_strong_matching = context.should_use_strong_matching;

  base::test::TestFuture<ActorLoginFlowVerifier::Result> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(),
      url::Origin::Create(GURL("https://a.example.com")),
      url::Origin::Create(GURL("https://b.example.com")), context_origin,
      should_use_strong_matching,
      MakeConsumeContextCallback(std::move(context)), future.GetCallback());

  EXPECT_EQ(future.Get(), ActorLoginFlowVerifier::Result::kPslMatchDisallowed);
  EXPECT_FALSE(ActorLoginFlowVerifier::IsSuccess(future.Get()));
}

TEST_F(ActorLoginFlowVerifierTest,
       GroupedOrigin_ReturnsGroupedOrOtherMismatch) {
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
  url::Origin context_origin = context.origin;
  bool should_use_strong_matching = context.should_use_strong_matching;

  base::test::TestFuture<ActorLoginFlowVerifier::Result> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(),
      url::Origin::Create(GURL("https://grouped.com")),
      main_rfh()->GetLastCommittedOrigin(), context_origin,
      should_use_strong_matching,
      MakeConsumeContextCallback(std::move(context)), future.GetCallback());

  EXPECT_EQ(future.Get(),
            ActorLoginFlowVerifier::Result::kGroupedOrOtherMismatch);
  EXPECT_FALSE(ActorLoginFlowVerifier::IsSuccess(future.Get()));
}

TEST_F(ActorLoginFlowVerifierTest,
       MainFramePslMatch_ReturnsMainFrameOriginMismatch) {
  autofill::ActorLoginContext context(
      url::Origin::Create(GURL("https://b.example.com")),
      /*should_use_strong_matching=*/false,
      /*navigations_per_frame=*/{{main_rfh()->GetFrameTreeNodeId(), 1}});
  url::Origin context_origin = context.origin;
  bool should_use_strong_matching = context.should_use_strong_matching;

  base::test::TestFuture<ActorLoginFlowVerifier::Result> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(),
      url::Origin::Create(GURL("https://b.example.com")),
      url::Origin::Create(GURL("https://a.example.com")), context_origin,
      should_use_strong_matching,
      MakeConsumeContextCallback(std::move(context)), future.GetCallback());

  EXPECT_EQ(future.Get(),
            ActorLoginFlowVerifier::Result::kMainFrameOriginMismatch);
  EXPECT_FALSE(ActorLoginFlowVerifier::IsSuccess(future.Get()));
}

TEST_F(ActorLoginFlowVerifierTest,
       MainFrameGroupedOrigin_ReturnsMainFrameOriginMismatch) {
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
  url::Origin context_origin = context.origin;
  bool should_use_strong_matching = context.should_use_strong_matching;

  base::test::TestFuture<ActorLoginFlowVerifier::Result> future;
  verifier_->VerifyIsActorLoginFlow(
      main_rfh()->GetFrameTreeNodeId(),
      url::Origin::Create(GURL("https://grouped.com")),
      main_rfh()->GetLastCommittedOrigin(), context_origin,
      should_use_strong_matching,
      MakeConsumeContextCallback(std::move(context)), future.GetCallback());

  EXPECT_EQ(future.Get(),
            ActorLoginFlowVerifier::Result::kMainFrameOriginMismatch);
  EXPECT_FALSE(ActorLoginFlowVerifier::IsSuccess(future.Get()));
}

TEST(ActorLoginFlowVerifierResultTest, StreamOutput) {
  using Result = ActorLoginFlowVerifier::Result;
  EXPECT_EQ(base::ToString(Result::kExactMatchAllowed), "ExactMatchAllowed");
  EXPECT_EQ(base::ToString(Result::kAffiliatedMatchAllowed),
            "AffiliatedMatchAllowed");
  EXPECT_EQ(base::ToString(Result::kPslMatchAllowed), "PslMatchAllowed");
  EXPECT_EQ(base::ToString(Result::kNoActorLoginContext),
            "NoActorLoginContext");
  EXPECT_EQ(base::ToString(Result::kFrameNotInLoginContext),
            "FrameNotInLoginContext");
  EXPECT_EQ(base::ToString(Result::kAllFramesHaveTooManyNavigations),
            "AllFramesHaveTooManyNavigations");
  EXPECT_EQ(base::ToString(Result::kNoMatch), "NoMatch");
  EXPECT_EQ(base::ToString(Result::kPslMatchDisallowed), "PslMatchDisallowed");
  EXPECT_EQ(base::ToString(Result::kGroupedOrOtherMismatch),
            "GroupedOrOtherMismatch");
  EXPECT_EQ(base::ToString(Result::kMainFrameOriginMismatch),
            "MainFrameOriginMismatch");
}

}  // namespace actor

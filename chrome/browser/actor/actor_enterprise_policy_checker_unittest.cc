// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_enterprise_policy_checker.h"

#include <utility>

#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {
namespace {

class ActorEnterprisePolicyCheckerTest : public testing::Test {
 public:
  ActorEnterprisePolicyCheckerTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ActorEnterprisePolicyCheckerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile("profile");
    checker_ = std::make_unique<ActorEnterprisePolicyChecker>(profile_);
  }

  void TearDown() override {
    checker_.reset();
    profile_ = nullptr;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<ActorEnterprisePolicyChecker> checker_;
};

TEST_F(ActorEnterprisePolicyCheckerTest, EvaluateDefaultUnblocked) {
  EXPECT_EQ(checker_->Evaluate(GURL("https://example.com/page")),
            EnterprisePolicyChecker::UrlBlockReason::kNotBlocked);
}

TEST_F(ActorEnterprisePolicyCheckerTest, EvaluateUrlBlocklistAndAllowlist) {
  base::test::TestFuture<void> on_url_lists_updated;
  base::CallbackListSubscription subscription =
      checker_->AddUrlListsUpdateObserver(
          on_url_lists_updated.GetRepeatingCallback());

  base::ListValue blocklist;
  blocklist.Append("blocked.example.com");
  profile_->GetPrefs()->SetList(glic::prefs::kGlicActuationOnWebBlockedForURLs,
                                std::move(blocklist));

  base::ListValue allowlist;
  allowlist.Append("allowed.blocked.example.com");
  profile_->GetPrefs()->SetList(glic::prefs::kGlicActuationOnWebAllowedForURLs,
                                std::move(allowlist));
  ASSERT_TRUE(on_url_lists_updated.Wait());

  EXPECT_EQ(checker_->Evaluate(GURL("https://blocked.example.com/path")),
            EnterprisePolicyChecker::UrlBlockReason::kExplicitlyBlocked);
  EXPECT_EQ(
      checker_->Evaluate(GURL("https://allowed.blocked.example.com/path")),
      EnterprisePolicyChecker::UrlBlockReason::kExplicitlyAllowed);
  EXPECT_EQ(checker_->Evaluate(GURL("https://other.example.com/path")),
            EnterprisePolicyChecker::UrlBlockReason::kNotBlocked);
}

TEST_F(ActorEnterprisePolicyCheckerTest,
       EvaluateManagedBrowserRespectsActuationPolicy) {
  policy::ScopedManagementServiceOverrideForTesting management_override(
      policy::ManagementServiceFactory::GetForProfile(profile_),
      policy::EnterpriseManagementAuthority::CLOUD);

  profile_->GetPrefs()->SetInteger(
      glic::prefs::kGlicActuationOnWeb,
      std::to_underlying(
          glic::prefs::GlicActuationOnWebPolicyState::kDisabled));

  base::test::TestFuture<void> on_url_lists_updated;
  base::CallbackListSubscription subscription =
      checker_->AddUrlListsUpdateObserver(
          on_url_lists_updated.GetRepeatingCallback());

  base::ListValue allowlist;
  allowlist.Append("allowed.example.com");
  profile_->GetPrefs()->SetList(glic::prefs::kGlicActuationOnWebAllowedForURLs,
                                std::move(allowlist));
  ASSERT_TRUE(on_url_lists_updated.Wait());

  EXPECT_EQ(checker_->Evaluate(GURL("https://unlisted.example.com/path")),
            EnterprisePolicyChecker::UrlBlockReason::kExplicitlyBlocked);
  EXPECT_EQ(checker_->Evaluate(GURL("https://allowed.example.com/path")),
            EnterprisePolicyChecker::UrlBlockReason::kExplicitlyAllowed);

  profile_->GetPrefs()->SetInteger(
      glic::prefs::kGlicActuationOnWeb,
      std::to_underlying(glic::prefs::GlicActuationOnWebPolicyState::kEnabled));
  EXPECT_EQ(checker_->Evaluate(GURL("https://unlisted.example.com/path")),
            EnterprisePolicyChecker::UrlBlockReason::kNotBlocked);
}

TEST_F(ActorEnterprisePolicyCheckerTest,
       ValidateContentSentToRendererNullFrameReturnsAllowed) {
  base::test::TestFuture<EnterprisePolicyChecker::ContentValidationReason>
      future;
  checker_->ValidateContentSentToRenderer(nullptr, "clipboard content",
                                          future.GetCallback());
  EXPECT_EQ(future.Get(),
            EnterprisePolicyChecker::ContentValidationReason::kAllowed);
}

}  // namespace
}  // namespace actor

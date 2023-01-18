// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_state.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_browsing_data_remover_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

// Enables or disables a base::Feature.
class ScopedInitFeature {
 public:
  explicit ScopedInitFeature(const base::Feature& feature, bool enable) {
    if (enable) {
      feature_list_.InitAndEnableFeature(feature);
    } else {
      feature_list_.InitAndDisableFeature(feature);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Enables/disables the DIPS Feature and updates DIPSServiceFactory's
// ProfileSelections to match.
class ScopedInitDIPSFeature {
 public:
  explicit ScopedInitDIPSFeature(bool enable)
      // DIPSServiceFactory is a singleton, and we want to create it *before*
      // constructing `init_feature_`, so that DIPSServiceFactory is initialized
      // using the default value of dips::kFeature. We only want `init_feature_`
      // to affect CreateProfileSelections(). We do this concisely by using the
      // comma operator in the arguments to `init_feature_` to call
      // DIPSServiceFactory::GetInstance() while ignoring its return value.
      : init_feature_((DIPSServiceFactory::GetInstance(), dips::kFeature),
                      enable),
        override_profile_selections_(
            DIPSServiceFactory::GetInstance(),
            DIPSServiceFactory::CreateProfileSelections()) {}

 private:
  ScopedInitFeature init_feature_;
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      override_profile_selections_;
};

using StateForURLCallback = base::OnceCallback<void(DIPSState)>;

class DIPSServiceTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DIPSServiceTest, CreateServiceIfFeatureEnabled) {
  ScopedInitDIPSFeature init_dips(true);

  TestingProfile profile;
  EXPECT_NE(DIPSService::Get(&profile), nullptr);
}

TEST_F(DIPSServiceTest, DontCreateServiceIfFeatureDisabled) {
  ScopedInitDIPSFeature init_dips(false);

  TestingProfile profile;
  EXPECT_EQ(DIPSService::Get(&profile), nullptr);
}

class DIPSServiceStateRemovalTest : public testing::Test {
 public:
  DIPSServiceStateRemovalTest()
      : base_profile_(std::make_unique<TestingProfile>()),
        incognito_profile_(
            TestingProfile::Builder().BuildIncognito(base_profile_.get())) {}

  base::TimeDelta grace_period;
  base::TimeDelta interaction_ttl;
  base::TimeDelta tiny_delta = base::Milliseconds(1);

  // These methods rely on the incognito profile since third-party cookies are
  // blocked by default in incognito and having them blocked is a condition for
  // DIPS to perform deletion.
  DIPSService* GetService() { return service_; }
  Profile* GetProfile() { return incognito_profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::MockBrowsingDataRemoverDelegate delegate_;

  // Test setup.
  void SetUp() override {
    grace_period = dips::kGracePeriod.Get();
    interaction_ttl = dips::kInteractionTtl.Get();
    ASSERT_LT(tiny_delta, grace_period);

    incognito_profile_->GetBrowsingDataRemover()->SetEmbedderDelegate(
        &delegate_);

    service_ = DIPSService::Get(incognito_profile_);
    DCHECK(service_);
    service_->SetStorageClockForTesting(&clock_);
    service_->storage()->FlushPostedTasksForTesting();
  }

  void TearDown() override {
    base_profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void WaitOnStorage() { service_->storage()->FlushPostedTasksForTesting(); }

  void AdvanceTimeTo(base::Time now) {
    ASSERT_GE(now, clock_.Now());
    clock_.SetNow(now);
  }

  void AdvanceTimeBy(base::TimeDelta delta) { clock_.Advance(delta); }

  void FireDIPSTimer() {
    service_->OnTimerFiredForTesting();
    WaitOnStorage();
  }

  void StateForURL(const GURL& url, StateForURLCallback callback) {
    service_->storage()
        ->AsyncCall(&DIPSStorage::Read)
        .WithArgs(url)
        .Then(std::move(callback));
  }

  absl::optional<StateValue> GetDIPSState(const GURL& url) {
    absl::optional<StateValue> state;
    StateForURL(url, base::BindLambdaForTesting([&](DIPSState loaded_state) {
                  if (loaded_state.was_loaded()) {
                    state = loaded_state.ToStateValue();
                  }
                }));
    WaitOnStorage();

    return state;
  }

 private:
  base::SimpleTestClock clock_;
  std::unique_ptr<TestingProfile> base_profile_;
  raw_ptr<TestingProfile> incognito_profile_ = nullptr;
  raw_ptr<DIPSService> service_ = nullptr;
};

// NOTE: The use of a MockBrowsingDataRemoverDelegate in this test fixture
// means that when DIPS deletion is enabled, the row for 'url' is not actually
// removed from the DIPS db since 'delegate_' doesn't actually carryout the
// removal task.
TEST_F(DIPSServiceStateRemovalTest, BrowsingDataDeletion_Enabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      dips::kFeature, {{"delete", "true"}, {"triggering_action", "bounce"}});

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce = base::Time::FromDoubleT(2);
  GetService()
      ->storage()
      ->AsyncCall(&DIPSStorage::RecordBounce)
      .WithArgs(url, bounce, false);
  WaitOnStorage();
  EXPECT_TRUE(GetDIPSState(url).has_value());

  // Set the current time to just after the bounce happened.
  AdvanceTimeTo(bounce + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify a removal task was not posted to the BrowsingDataRemover(Delegate).
  delegate_.VerifyAndClearExpectations();

  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddRegisterableDomain(GetSiteForDIPS(url));
  delegate_.ExpectCall(
      base::Time::Min(), base::Time::Max(),
      chrome_browsing_data_remover::FILTERABLE_DATA_TYPES |
          content::BrowsingDataRemover::DATA_TYPE_AVOID_CLOSING_CONNECTIONS,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
          content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
      filter_builder.get());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce + grace_period + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify that a removal task was posted to the BrowsingDataRemover(Delegate)
  // for 'url'.
  delegate_.VerifyAndClearExpectations();
  // Because this test fixture uses a MockBrowsingDataRemoverDelegate the DIPS
  // entry should not actually be removed. However, in practice it would be.
  EXPECT_TRUE(GetDIPSState(url).has_value());
}

TEST_F(DIPSServiceStateRemovalTest, BrowsingDataDeletion_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      dips::kFeature, {{"delete", "false"}, {"triggering_action", "bounce"}});

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce = base::Time::FromDoubleT(2);
  GetService()
      ->storage()
      ->AsyncCall(&DIPSStorage::RecordBounce)
      .WithArgs(url, bounce, false);
  WaitOnStorage();
  EXPECT_TRUE(GetDIPSState(url).has_value());

  // Set the current time to just after the bounce happened.
  AdvanceTimeTo(bounce + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify the DIPS entry was not removed and a removal task was not posted to
  // the BrowsingDataRemover(Delegate).
  delegate_.VerifyAndClearExpectations();
  EXPECT_TRUE(GetDIPSState(url).has_value());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce + grace_period + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify that the site's DIPS entry WAS removed, but a removal task was NOT
  // posted to the BrowsingDataRemover(Delegate) since `dips::kDeletionEnabled`
  // is false.
  delegate_.VerifyAndClearExpectations();
  EXPECT_FALSE(GetDIPSState(url).has_value());
}

// A test class that verifies DIPSService state deletion metrics collection
// behavior.
class DIPSServiceHistogramTest : public DIPSServiceStateRemovalTest {
 public:
  DIPSServiceHistogramTest() = default;

  const base::HistogramTester& histograms() const { return histogram_tester_; }

 protected:
  base::HistogramTester histogram_tester_;
};

TEST_F(DIPSServiceHistogramTest, DeletionLatency) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      dips::kFeature, {{"delete", "false"}, {"triggering_action", "bounce"}});

  // Verify the histogram starts empty
  histograms().ExpectTotalCount("Privacy.DIPS.DeletionLatency", 0);

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce = base::Time::FromDoubleT(2);
  GetService()
      ->storage()
      ->AsyncCall(&DIPSStorage::RecordBounce)
      .WithArgs(url, bounce, false);
  WaitOnStorage();

  // Set the current time to just after the bounce happened.
  AdvanceTimeTo(bounce + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify deletion latency metrics were NOT emitted and the DIPS entry was NOT
  // removed.
  histograms().ExpectTotalCount("Privacy.DIPS.DeletionLatency", 0);
  EXPECT_TRUE(GetDIPSState(url).has_value());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce + grace_period + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify a deletion latency metric was emitted and the DIPS entry was
  // removed.
  histograms().ExpectTotalCount("Privacy.DIPS.DeletionLatency", 1);
  EXPECT_FALSE(GetDIPSState(url).has_value());
}

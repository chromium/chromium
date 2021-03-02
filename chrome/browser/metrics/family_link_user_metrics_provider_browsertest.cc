// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_link_user_metrics_provider.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/child_accounts/family_features.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/metrics_service.h"
#include "content/public/test/browser_test.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {

// Returns the user type for logging in.
chromeos::LoggedInUserMixin::LogInType GetLogInType(
    FamilyLinkUserMetricsProvider::LogSegment log_segment) {
  switch (log_segment) {
    case FamilyLinkUserMetricsProvider::LogSegment::kOther:
      return chromeos::LoggedInUserMixin::LogInType::kRegular;
    case FamilyLinkUserMetricsProvider::LogSegment::kUnderConsentAge:
    case FamilyLinkUserMetricsProvider::LogSegment::kOverConsentAge:
      return chromeos::LoggedInUserMixin::LogInType::kChild;
  }
}

void ProvideCurrentSessionData() {
  // The purpose of the below call is to avoid a DCHECK failure in an unrelated
  // metrics provider, in |FieldTrialsProvider::ProvideCurrentSessionData()|.
  metrics::SystemProfileProto system_profile_proto;
  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->ProvideSystemProfileMetricsWithLogCreationTime(base::TimeTicks::Now(),
                                                       &system_profile_proto);
  metrics::ChromeUserMetricsExtension uma_proto;
  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->ProvideCurrentSessionData(&uma_proto);
}

}  // namespace

class FamilyLinkUserMetricsProviderForTesting
    : public FamilyLinkUserMetricsProvider {
 public:
  FamilyLinkUserMetricsProviderForTesting() = default;
  FamilyLinkUserMetricsProviderForTesting(
      const FamilyLinkUserMetricsProviderForTesting&) = delete;
  FamilyLinkUserMetricsProviderForTesting& operator=(
      const FamilyLinkUserMetricsProviderForTesting&) = delete;
  ~FamilyLinkUserMetricsProviderForTesting() override = default;

  void SetRunLoopQuitClosure(base::RepeatingClosure closure) {
    quit_closure_ = base::BindOnce(closure);
  }

 private:
  void SetLogSegment(LogSegment log_segment) override {
    FamilyLinkUserMetricsProvider::SetLogSegment(log_segment);
    std::move(quit_closure_).Run();
  }

  base::OnceClosure quit_closure_;
};

class FamilyLinkUserMetricsProviderTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<
          FamilyLinkUserMetricsProvider::LogSegment> {
 public:
  FamilyLinkUserMetricsProviderTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::kFamilyLinkUserMetricsProvider);
  }

 protected:
  chromeos::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, GetLogInType(GetParam()), embedded_test_server(),
      /*test_base=*/this};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(FamilyLinkUserMetricsProviderTest, UserCategory) {
  base::HistogramTester histogram_tester;
  FamilyLinkUserMetricsProviderForTesting provider;
  base::RunLoop run_loop;

  // Simulate calling ProvideCurrentSessionData() prior to logging in. This call
  // should return prematurely.
  provider.ProvideCurrentSessionData(/*uma_proto_unused=*/nullptr);

  // No metrics were recorded.
  histogram_tester.ExpectTotalCount(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(), 0);

  provider.SetRunLoopQuitClosure(run_loop.QuitClosure());

  const FamilyLinkUserMetricsProvider::LogSegment log_segment = GetParam();
  // Set up service flags for children under the age of consent.
  logged_in_user_mixin_.GetFakeGaiaMixin()->set_initialize_child_id_token(
      log_segment ==
      FamilyLinkUserMetricsProvider::LogSegment::kUnderConsentAge);
  logged_in_user_mixin_.LogInUser(/*issue_any_scope_token=*/true);

  run_loop.Run();

  // Simulate calling ProvideCurrentSessionData() after logging in.
  provider.ProvideCurrentSessionData(/*uma_proto_unused=*/nullptr);

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(), log_segment,
      1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FamilyLinkUserMetricsProviderTest,
    testing::Values(
        FamilyLinkUserMetricsProvider::LogSegment::kOther,
        FamilyLinkUserMetricsProvider::LogSegment::kUnderConsentAge,
        FamilyLinkUserMetricsProvider::LogSegment::kOverConsentAge));

class FamilyLinkUserMetricsProviderGuestModeTest
    : public MixinBasedInProcessBrowserTest {
 public:
  FamilyLinkUserMetricsProviderGuestModeTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::kFamilyLinkUserMetricsProvider);
  }

 private:
  chromeos::GuestSessionMixin guest_session_mixin_{&mixin_host_};

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that guest users go into the kOther bucket.
IN_PROC_BROWSER_TEST_F(FamilyLinkUserMetricsProviderGuestModeTest, GuestMode) {
  base::HistogramTester histogram_tester;

  ProvideCurrentSessionData();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kOther, 1);
}

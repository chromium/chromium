// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chromeos_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_service.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {

// Returns the user type for logging in.
ash::LoggedInUserMixin::LogInType GetLogInType(
    user_manager::UserType user_type) {
  if (user_type == user_manager::USER_TYPE_CHILD)
    return ash::LoggedInUserMixin::LogInType::kChild;
  return ash::LoggedInUserMixin::LogInType::kRegular;
}

void ProvideHistograms(bool should_emit_histograms_earlier) {
  // The purpose of the below call is to avoid a DCHECK failure in an
  // unrelated metrics provider, in
  // |FieldTrialsProvider::ProvideCurrentSessionData()|.
  metrics::SystemProfileProto system_profile_proto;
  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->ProvideSystemProfileMetricsWithLogCreationTime(base::TimeTicks::Now(),
                                                       &system_profile_proto);
  if (!should_emit_histograms_earlier) {
    metrics::ChromeUserMetricsExtension uma_proto;
    g_browser_process->metrics_service()
        ->GetDelegatingProviderForTesting()
        ->ProvideCurrentSessionData(&uma_proto);
  } else {
    g_browser_process->metrics_service()
        ->GetDelegatingProviderForTesting()
        ->OnDidCreateMetricsLog();
  }
}

}  // namespace

struct ChromeOSMetricsProviderTestParams {
  user_manager::UserType user_type;
  bool emit_histograms_earlier;
};

class ChromeOSMetricsProviderTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<ChromeOSMetricsProviderTestParams> {
 public:
  void SetUp() override {
    if (ShouldEmitHistogramsEarlier()) {
      feature_list_.InitWithFeatures(
          {metrics::features::kEmitHistogramsEarlier}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {metrics::features::kEmitHistogramsEarlier});
    }

    MixinBasedInProcessBrowserTest::SetUp();
  }

 protected:
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, GetLogInType(GetParam().user_type), embedded_test_server(),
      this};

  bool ShouldEmitHistogramsEarlier() const {
    return GetParam().emit_histograms_earlier;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ChromeOSMetricsProviderTest, PrimaryUserType) {
  base::HistogramTester histogram_tester;

  // Simulate calling ProvideHistograms() prior to logging in. This call should
  // not record any UMA.PrimaryUserType metrics.
  ProvideHistograms(ShouldEmitHistogramsEarlier());

  // No metrics were recorded.
  histogram_tester.ExpectTotalCount("UMA.PrimaryUserType", 0);

  logged_in_user_mixin_.LogInUser();

  // Simulate calling ProvideHistograms() after logging in.
  ProvideHistograms(ShouldEmitHistogramsEarlier());

  user_manager::UserType user_type = GetParam().user_type;
  histogram_tester.ExpectUniqueSample("UMA.PrimaryUserType", user_type, 1);
}

INSTANTIATE_TEST_SUITE_P(,
                         ChromeOSMetricsProviderTest,
                         testing::Values(
                             ChromeOSMetricsProviderTestParams{
                                 .user_type = user_manager::USER_TYPE_REGULAR,
                                 .emit_histograms_earlier = true},
                             ChromeOSMetricsProviderTestParams{
                                 .user_type = user_manager::USER_TYPE_CHILD,
                                 .emit_histograms_earlier = true},
                             ChromeOSMetricsProviderTestParams{
                                 .user_type = user_manager::USER_TYPE_REGULAR,
                                 .emit_histograms_earlier = false},
                             ChromeOSMetricsProviderTestParams{
                                 .user_type = user_manager::USER_TYPE_CHILD,
                                 .emit_histograms_earlier = false}));

class ChromeOSMetricsProviderGuestModeTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    if (ShouldEmitHistogramsEarlier()) {
      feature_list_.InitWithFeatures(
          {metrics::features::kEmitHistogramsEarlier}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {metrics::features::kEmitHistogramsEarlier});
    }

    MixinBasedInProcessBrowserTest::SetUp();
  }

  bool ShouldEmitHistogramsEarlier() const { return GetParam(); }

 private:
  ash::GuestSessionMixin guest_session_mixin_{&mixin_host_};
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeOSMetricsProviderGuestModeTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(ChromeOSMetricsProviderGuestModeTest, PrimaryUserType) {
  base::HistogramTester histogram_tester;

  ProvideHistograms(ShouldEmitHistogramsEarlier());

  histogram_tester.ExpectUniqueSample("UMA.PrimaryUserType",
                                      user_manager::USER_TYPE_GUEST, 1);
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chromeos_family_link_user_metrics_provider.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/metrics_service.h"
#include "content/public/test/browser_test.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {

// Returns the user type for logging in.
ash::LoggedInUserMixin::LogInType GetLogInType(
    ChromeOSFamilyLinkUserMetricsProvider::LogSegment log_segment) {
  switch (log_segment) {
    case ChromeOSFamilyLinkUserMetricsProvider::LogSegment::kOther:
      return ash::LoggedInUserMixin::LogInType::kConsumer;
    case ChromeOSFamilyLinkUserMetricsProvider::LogSegment::kUnderConsentAge:
    case ChromeOSFamilyLinkUserMetricsProvider::LogSegment::kOverConsentAge:
      return ash::LoggedInUserMixin::LogInType::kChild;
  }
}

void ProvideHistograms() {
  // The purpose of the below call is to avoid a DCHECK failure in an unrelated
  // metrics provider, in |FieldTrialsProvider::ProvideCurrentSessionData()|.
  metrics::SystemProfileProto system_profile_proto;
  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->ProvideSystemProfileMetricsWithLogCreationTime(base::TimeTicks::Now(),
                                                       &system_profile_proto);
  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->OnDidCreateMetricsLog();
}

}  // namespace

class ChromeOSFamilyLinkUserMetricsProviderForTesting
    : public ChromeOSFamilyLinkUserMetricsProvider {
 public:
  ChromeOSFamilyLinkUserMetricsProviderForTesting() = default;
  ChromeOSFamilyLinkUserMetricsProviderForTesting(
      const ChromeOSFamilyLinkUserMetricsProviderForTesting&) = delete;
  ChromeOSFamilyLinkUserMetricsProviderForTesting& operator=(
      const ChromeOSFamilyLinkUserMetricsProviderForTesting&) = delete;
  ~ChromeOSFamilyLinkUserMetricsProviderForTesting() override = default;

  void SetRunLoopQuitClosure(base::RepeatingClosure closure) {
    quit_closure_ = base::BindOnce(closure);
  }

 private:
  void SetLogSegment(LogSegment log_segment) override {
    ChromeOSFamilyLinkUserMetricsProvider::SetLogSegment(log_segment);
    std::move(quit_closure_).Run();
  }

  base::OnceClosure quit_closure_;
};

class ChromeOSFamilyLinkUserMetricsProviderTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<
          ChromeOSFamilyLinkUserMetricsProvider::LogSegment> {
 protected:
  ash::LoggedInUserMixin logged_in_user_mixin_{&mixin_host_, /*test_base=*/this,
                                               embedded_test_server(),
                                               GetLogInType(GetParam())};
};

IN_PROC_BROWSER_TEST_P(ChromeOSFamilyLinkUserMetricsProviderTest,
                       UserCategory) {
  base::HistogramTester histogram_tester;
  ChromeOSFamilyLinkUserMetricsProviderForTesting provider;
  base::RunLoop run_loop;

  // Simulate calling ProvideHistograms() prior to logging in. This call should
  // return prematurely.
  ProvideHistograms();

  // No metrics were recorded.
  histogram_tester.ExpectTotalCount(
      ChromeOSFamilyLinkUserMetricsProvider::GetHistogramNameForTesting(), 0);

  provider.SetRunLoopQuitClosure(run_loop.QuitClosure());

  const ChromeOSFamilyLinkUserMetricsProvider::LogSegment log_segment =
      GetParam();
  // Set up service flags for children under the age of consent.
  logged_in_user_mixin_.GetFakeGaiaMixin()->set_initialize_child_id_token(
      log_segment ==
      ChromeOSFamilyLinkUserMetricsProvider::LogSegment::kUnderConsentAge);
  logged_in_user_mixin_.LogInUser(
      {ash::LoggedInUserMixin::LoginDetails::kUseAnyScopeToken});

  run_loop.Run();

  // Simulate calling ProvideHistograms() after logging in.
  ProvideHistograms();

  histogram_tester.ExpectUniqueSample(
      ChromeOSFamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      log_segment, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ChromeOSFamilyLinkUserMetricsProviderTest,
    testing::Values(
        ChromeOSFamilyLinkUserMetricsProvider::LogSegment::kOther,
        ChromeOSFamilyLinkUserMetricsProvider::LogSegment::kUnderConsentAge,
        ChromeOSFamilyLinkUserMetricsProvider::LogSegment::kOverConsentAge));

class ChromeOSFamilyLinkUserMetricsProviderGuestModeTest
    : public MixinBasedInProcessBrowserTest {
 private:
  ash::GuestSessionMixin guest_session_mixin_{&mixin_host_};
};

// Tests that guest users go into the kOther bucket.
IN_PROC_BROWSER_TEST_F(ChromeOSFamilyLinkUserMetricsProviderGuestModeTest,
                       GuestMode) {
  base::HistogramTester histogram_tester;

  ProvideHistograms();

  histogram_tester.ExpectUniqueSample(
      ChromeOSFamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      ChromeOSFamilyLinkUserMetricsProvider::LogSegment::kOther, 1);
}

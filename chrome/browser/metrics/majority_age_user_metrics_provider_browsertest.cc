// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/testing/sync_metrics_test_utils.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/demographics/demographic_metrics_provider.h"
#include "components/metrics/demographics/demographic_metrics_test_utils.h"
#include "components/metrics/demographics/user_demographics.h"
#include "components/metrics/metrics_service.h"
#include "components/sync/test/fake_server.h"
#include "content/public/test/browser_test.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

namespace chromeos {

namespace {

// Explicitly calls ProvideCurrentSessionData() for all metrics providers.
void ProvideCurrentSessionData() {
  // The purpose of the below call is to avoid a DCHECK failure in an
  // unrelated metrics provider, in
  // |FieldTrialsProvider::ProvideCurrentSessionData()|.
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

class MajorityAgeUserMetricsProviderTest
    : public SyncTest,
      public testing::WithParamInterface</*age=*/int> {
 public:
  MajorityAgeUserMetricsProviderTest() : SyncTest(SINGLE_CLIENT) {
    scoped_feature_list_.InitAndEnableFeature(
        metrics::kDemographicMetricsReporting);
  }

  int GetAge() { return GetParam(); }

  PrefService* local_state() { return g_browser_process->local_state(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(MajorityAgeUserMetricsProviderTest,
                       IsUserOver21Under85) {
  base::HistogramTester histogram_tester;

  // Set network time for test.
  const base::Time now = base::Time::Now();
  metrics::test::UpdateNetworkTime(now,
                                   g_browser_process->network_time_tracker());

  // Compute birth year.
  base::Time::Exploded exploded_now_time;
  now.LocalExplode(&exploded_now_time);
  int test_birth_year = exploded_now_time.year - GetAge();
  metrics::UserDemographicsProto::Gender test_gender =
      metrics::UserDemographicsProto::GENDER_MALE;
  // Assign a random gender.
  if (test_birth_year % 2 == 0)
    test_gender = metrics::UserDemographicsProto::GENDER_FEMALE;

  metrics::test::AddUserBirthYearAndGenderToSyncServer(
      GetFakeServer()->AsWeakPtr(), test_birth_year, test_gender);

  // TODO(crbug.com/40688248): Try to replace the below set-up code with
  // functions from SyncTest.
  std::unique_ptr<SyncServiceImplHarness> harness =
      metrics::test::InitializeProfileForSync(browser()->profile(),
                                              GetFakeServer()->AsWeakPtr());
  ASSERT_TRUE(harness->SetupSync());

  // Simulate calling ProvideCurrentSessionData() after logging in.
  ProvideCurrentSessionData();

  const int noised_age =
      exploded_now_time.year -
      metrics::test::GetNoisedBirthYear(local_state(), test_birth_year);
  const bool is_eligible =
      noised_age > metrics::kUserDemographicsMinAgeInYears &&
      noised_age <= metrics::kUserDemographicsMaxAgeInYears;
  histogram_tester.ExpectUniqueSample(
      "UMA.UserDemographics.IsNoisedAgeOver21Under85",
      /*sample=*/is_eligible,
      /*expected_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    MajorityAgeUserMetricsProviderTest,
    testing::Values(0, 12, 13, 14, 17, 18, 19, 20, 21, 22, 83, 84, 85, 86));

class MajorityAgeUserMetricsProviderGuestModeTest
    : public MixinBasedInProcessBrowserTest {
 public:
  MajorityAgeUserMetricsProviderGuestModeTest() {
    scoped_feature_list_.InitAndEnableFeature(
        metrics::kDemographicMetricsReporting);
  }

 private:
  ash::GuestSessionMixin guest_session_mixin_{&mixin_host_};

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that guest users report unknown data because they don't have an age and
// don't crash.
IN_PROC_BROWSER_TEST_F(MajorityAgeUserMetricsProviderGuestModeTest, GuestMode) {
  base::HistogramTester histogram_tester;

  ProvideCurrentSessionData();

  histogram_tester.ExpectUniqueSample(
      "UMA.UserDemographics.IsNoisedAgeOver21Under85",
      /*sample=*/false,  // unknown
      /*expected_count=*/1);
}

}  // namespace chromeos

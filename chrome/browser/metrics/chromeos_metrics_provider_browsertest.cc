// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chromeos_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/enrollment_status.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {

using UkmEntry = ukm::builders::ChromeOS_DeviceManagement;

// Returns the user type for logging in.
ash::LoggedInUserMixin::LogInType GetLogInType(
    user_manager::UserType user_type) {
  if (user_type == user_manager::UserType::kChild) {
    return ash::LoggedInUserMixin::LogInType::kChild;
  }
  return ash::LoggedInUserMixin::LogInType::kConsumer;
}

void ProvideHistograms() {
  // The purpose of the below call is to avoid a DCHECK failure in an
  // unrelated metrics provider, in
  // |FieldTrialsProvider::ProvideCurrentSessionData()|.
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

class ChromeOSMetricsProviderTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<user_manager::UserType> {
 protected:
  ash::LoggedInUserMixin logged_in_user_mixin_{&mixin_host_, /*test_base=*/this,
                                               embedded_test_server(),
                                               GetLogInType(GetParam())};
};

IN_PROC_BROWSER_TEST_P(ChromeOSMetricsProviderTest, PrimaryUserType) {
  base::HistogramTester histogram_tester;

  // Simulate calling ProvideHistograms() prior to logging in. This call should
  // not record any UMA.PrimaryUserType metrics.
  ProvideHistograms();

  // No metrics were recorded.
  histogram_tester.ExpectTotalCount("UMA.PrimaryUserType", 0);

  logged_in_user_mixin_.LogInUser();

  // Simulate calling ProvideHistograms() after logging in.
  ProvideHistograms();

  user_manager::UserType user_type = GetParam();
  histogram_tester.ExpectUniqueSample("UMA.PrimaryUserType", user_type, 1);
}

INSTANTIATE_TEST_SUITE_P(,
                         ChromeOSMetricsProviderTest,
                         testing::Values(user_manager::UserType::kRegular,
                                         user_manager::UserType::kChild));

class ChromeOSMetricsProviderGuestModeTest
    : public MixinBasedInProcessBrowserTest {
 private:
  ash::GuestSessionMixin guest_session_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(ChromeOSMetricsProviderGuestModeTest, PrimaryUserType) {
  base::HistogramTester histogram_tester;

  ProvideHistograms();

  histogram_tester.ExpectUniqueSample("UMA.PrimaryUserType",
                                      user_manager::UserType::kGuest, 1);
}

class ChromeOSMetricsProviderEnrolledDeviceTest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  ChromeOSMetricsProviderEnrolledDeviceTest() {
    device_state_.set_skip_initial_policy_setup(true);
    device_state_.SetState(
        ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
  }

  ~ChromeOSMetricsProviderEnrolledDeviceTest() override = default;
};

// Test that the UKM event is recorded with the correct value when the device is
// managed.
IN_PROC_BROWSER_TEST_F(ChromeOSMetricsProviderEnrolledDeviceTest,
                       ProvideCurrentSessionUKMData) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->ProvideCurrentSessionUKMData();

  auto ukm_entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[0], UkmEntry::kEnrollmentStatusName,
      static_cast<int>(EnrollmentStatus::kManaged));
}

class ChromeOSMetricsProviderConsumerOwnedDeviceTest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  ChromeOSMetricsProviderConsumerOwnedDeviceTest() {
    device_state_.set_skip_initial_policy_setup(true);
    device_state_.SetState(
        ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED);
  }

  ~ChromeOSMetricsProviderConsumerOwnedDeviceTest() override = default;
};
// Test that the UKM event is recorded with the correct value when the device is
// not managed.
IN_PROC_BROWSER_TEST_F(ChromeOSMetricsProviderConsumerOwnedDeviceTest,
                       ProvideCurrentSessionUKMData) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->ProvideCurrentSessionUKMData();

  auto ukm_entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[0], UkmEntry::kEnrollmentStatusName,
      static_cast<int>(EnrollmentStatus::kNonManaged));
}

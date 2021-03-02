// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chromeos_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {

// Returns the user type for logging in.
chromeos::LoggedInUserMixin::LogInType GetLogInType(
    user_manager::UserType user_type) {
  if (user_type == user_manager::USER_TYPE_CHILD)
    return chromeos::LoggedInUserMixin::LogInType::kChild;
  return chromeos::LoggedInUserMixin::LogInType::kRegular;
}

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

class ChromeOSMetricsProviderTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<user_manager::UserType> {
 protected:
  chromeos::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, GetLogInType(GetParam()), embedded_test_server(), this};
};

IN_PROC_BROWSER_TEST_P(ChromeOSMetricsProviderTest, PrimaryUserType) {
  base::HistogramTester histogram_tester;

  // Simulate calling ProvideCurrentSessionData() prior to logging in. This call
  // should not record any UMA.PrimaryUserType metrics.
  ProvideCurrentSessionData();

  // No metrics were recorded.
  histogram_tester.ExpectTotalCount("UMA.PrimaryUserType", 0);

  logged_in_user_mixin_.LogInUser();

  // Simulate calling ProvideCurrentSessionData() after logging in.
  ProvideCurrentSessionData();

  user_manager::UserType user_type = GetParam();
  histogram_tester.ExpectUniqueSample("UMA.PrimaryUserType", user_type, 1);
}

INSTANTIATE_TEST_SUITE_P(,
                         ChromeOSMetricsProviderTest,
                         testing::Values(user_manager::USER_TYPE_REGULAR,
                                         user_manager::USER_TYPE_CHILD));

class ChromeOSMetricsProviderGuestModeTest
    : public MixinBasedInProcessBrowserTest {
 private:
  chromeos::GuestSessionMixin guest_session_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(ChromeOSMetricsProviderGuestModeTest, PrimaryUserType) {
  base::HistogramTester histogram_tester;

  ProvideCurrentSessionData();

  histogram_tester.ExpectUniqueSample("UMA.PrimaryUserType",
                                      user_manager::USER_TYPE_GUEST, 1);
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enterprise_user_session_metrics.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

AccountId GetAccountId() {
  return AccountId::FromUserEmail("fake-email@example.com");
}

}  // namespace

// Tests enterprise session start/stop metrics recording.
// TODO(michaelpg): Add browser tests to verify the methods are called at the
// right times.
class EnterpriseUserSessionMetricsTest : public testing::Test {
 public:
  EnterpriseUserSessionMetricsTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        install_attributes_(std::make_unique<ScopedStubInstallAttributes>(
            StubInstallAttributes::CreateCloudManaged("test-domain",
                                                      "FAKE_DEVICE_ID"))) {}
  ~EnterpriseUserSessionMetricsTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<ScopedStubInstallAttributes> install_attributes_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnterpriseUserSessionMetricsTest);
};

// Tests recording a sign-in event with a sign-in event type.
TEST_F(EnterpriseUserSessionMetricsTest, RecordSignInEvent1) {
  {
    SCOPED_TRACE("");
    base::HistogramTester histogram_tester;
    enterprise_user_session_metrics::RecordSignInEvent(
        enterprise_user_session_metrics::SignInEventType::
            AUTOMATIC_PUBLIC_SESSION);
    histogram_tester.ExpectUniqueSample(
        "Enterprise.UserSession.Logins",
        static_cast<int>(enterprise_user_session_metrics::SignInEventType::
                             AUTOMATIC_PUBLIC_SESSION),
        1);
  }
  {
    SCOPED_TRACE("");
    base::HistogramTester histogram_tester;
    enterprise_user_session_metrics::RecordSignInEvent(
        enterprise_user_session_metrics::SignInEventType::MANUAL_KIOSK);
    histogram_tester.ExpectUniqueSample(
        "Enterprise.UserSession.Logins",
        static_cast<int>(
            enterprise_user_session_metrics::SignInEventType::MANUAL_KIOSK),
        1);
  }
  {
    SCOPED_TRACE("");
    base::HistogramTester histogram_tester;
    enterprise_user_session_metrics::RecordSignInEvent(
        enterprise_user_session_metrics::SignInEventType::REGULAR_USER);
    histogram_tester.ExpectUniqueSample(
        "Enterprise.UserSession.Logins",
        static_cast<int>(
            enterprise_user_session_metrics::SignInEventType::REGULAR_USER),
        1);
  }
}

// Tests recording a sign-in event with a user context.
TEST_F(EnterpriseUserSessionMetricsTest, RecordSignInEvent2) {
  {
    SCOPED_TRACE("");
    base::HistogramTester histogram_tester;
    enterprise_user_session_metrics::RecordSignInEvent(
        UserContext(user_manager::USER_TYPE_REGULAR, GetAccountId()),
        false /* is_auto_login */);
    histogram_tester.ExpectUniqueSample(
        "Enterprise.UserSession.Logins",
        static_cast<int>(
            enterprise_user_session_metrics::SignInEventType::REGULAR_USER),
        1);
  }
  {
    SCOPED_TRACE("");
    base::HistogramTester histogram_tester;
    enterprise_user_session_metrics::RecordSignInEvent(
        UserContext(user_manager::USER_TYPE_PUBLIC_ACCOUNT, GetAccountId()),
        false /* is_auto_login */);
    histogram_tester.ExpectUniqueSample(
        "Enterprise.UserSession.Logins",
        static_cast<int>(enterprise_user_session_metrics::SignInEventType::
                             MANUAL_PUBLIC_SESSION),
        1);
  }
  {
    SCOPED_TRACE("");
    base::HistogramTester histogram_tester;
    enterprise_user_session_metrics::RecordSignInEvent(
        UserContext(user_manager::USER_TYPE_PUBLIC_ACCOUNT, GetAccountId()),
        true /* is_auto_login */);
    histogram_tester.ExpectUniqueSample(
        "Enterprise.UserSession.Logins",
        static_cast<int>(enterprise_user_session_metrics::SignInEventType::
                             AUTOMATIC_PUBLIC_SESSION),
        1);
  }
}

// Tests recording session length.
TEST_F(EnterpriseUserSessionMetricsTest, RecordSessionLength) {
  {
    SCOPED_TRACE("");
    base::HistogramTester histogram_tester;
    enterprise_user_session_metrics::StoreSessionLength(
        user_manager::UserType::USER_TYPE_PUBLIC_ACCOUNT,
        base::TimeDelta::FromMinutes(25));
    enterprise_user_session_metrics::RecordStoredSessionLength();

    // Time is rounded down to the nearest 10.
    histogram_tester.ExpectUniqueSample(
        "Enterprise.PublicSession.SessionLength", 20, 1);

    // No other session length metrics are recorded.
    histogram_tester.ExpectTotalCount(
        "Enterprise.RegularUserSession.SessionLength", 0);
    histogram_tester.ExpectTotalCount("DemoMode.SessionLength", 0);
  }
  {
    SCOPED_TRACE("");
    // Test with a regular user session.
    base::HistogramTester histogram_tester;
    enterprise_user_session_metrics::StoreSessionLength(
        user_manager::UserType::USER_TYPE_REGULAR,
        base::TimeDelta::FromMinutes(149));
    enterprise_user_session_metrics::RecordStoredSessionLength();
    histogram_tester.ExpectUniqueSample(
        "Enterprise.RegularUserSession.SessionLength", 140, 1);

    // No other session length metrics are recorded.
    histogram_tester.ExpectTotalCount("Enterprise.PublicSession.SessionLength",
                                      0);
    histogram_tester.ExpectTotalCount("DemoMode.SessionLength", 0);
  }
  {
    SCOPED_TRACE("");
    base::HistogramTester histogram_tester;
    enterprise_user_session_metrics::StoreSessionLength(
        user_manager::UserType::USER_TYPE_REGULAR,
        base::TimeDelta::FromDays(10));
    enterprise_user_session_metrics::RecordStoredSessionLength();

    // Reported length is capped at 24 hours.
    histogram_tester.ExpectUniqueSample(
        "Enterprise.RegularUserSession.SessionLength",
        base::TimeDelta::FromHours(24).InMinutes(), 1);

    // No other session length metrics are recorded.
    histogram_tester.ExpectTotalCount("Enterprise.PublicSession.SessionLength",
                                      0);
    histogram_tester.ExpectTotalCount("DemoMode.SessionLength", 0);
  }
  {
    SCOPED_TRACE("");
    // Test with no session. This verifies the same metric isn't recorded twice
    // if something goes wrong.
    base::HistogramTester histogram_tester;
    enterprise_user_session_metrics::RecordStoredSessionLength();
    histogram_tester.ExpectTotalCount("Enterprise.PublicSession.SessionLength",
                                      0);
    histogram_tester.ExpectTotalCount(
        "Enterprise.RegularUserSession.SessionLength", 0);
    histogram_tester.ExpectTotalCount("DemoMode.SessionLength", 0);
  }
}

// Tests recording session length in demo sessions, which includes an additional
// metric with tighter bucket spacing.
TEST_F(EnterpriseUserSessionMetricsTest, RecordDemoSessionLength) {
  DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kOnline);
  {
    SCOPED_TRACE("");
    base::HistogramTester histogram_tester;
    enterprise_user_session_metrics::StoreSessionLength(
        user_manager::UserType::USER_TYPE_PUBLIC_ACCOUNT,
        base::TimeDelta::FromSeconds(25 * 60 + 59));
    enterprise_user_session_metrics::RecordStoredSessionLength();

    // Time is rounded down to the nearest 10 minutes.
    histogram_tester.ExpectUniqueSample(
        "Enterprise.PublicSession.SessionLength", 20, 1);

    // The demo mode session length metric is rounded down to the nearest
    // minute.
    histogram_tester.ExpectUniqueSample("DemoMode.SessionLength", 25, 1);
  }
  {
    SCOPED_TRACE("");
    base::HistogramTester histogram_tester;
    enterprise_user_session_metrics::StoreSessionLength(
        user_manager::UserType::USER_TYPE_PUBLIC_ACCOUNT,
        base::TimeDelta::FromDays(10));
    enterprise_user_session_metrics::RecordStoredSessionLength();

    // Reported length is capped at 24 hours.
    histogram_tester.ExpectUniqueSample(
        "Enterprise.PublicSession.SessionLength",
        base::TimeDelta::FromHours(24).InMinutes(), 1);

    // Demo session length is capped at 2 hours because demo sessions are short.
    histogram_tester.ExpectUniqueSample(
        "DemoMode.SessionLength", base::TimeDelta::FromHours(2).InMinutes(), 1);
  }
  {
    SCOPED_TRACE("");
    // Test with no session. This verifies the same metric isn't recorded twice
    // if something goes wrong.
    base::HistogramTester histogram_tester;
    enterprise_user_session_metrics::RecordStoredSessionLength();
    histogram_tester.ExpectTotalCount("Enterprise.PublicSession.SessionLength",
                                      0);
    histogram_tester.ExpectTotalCount(
        "Enterprise.RegularUserSession.SessionLength", 0);
    histogram_tester.ExpectTotalCount("DemoMode.SessionLength", 0);
  }
}

}  // namespace chromeos

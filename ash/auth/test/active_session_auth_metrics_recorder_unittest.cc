// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/active_session_auth_metrics_recorder.h"

#include <memory>

#include "ash/auth/views/auth_common.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"

namespace ash {
namespace {

constexpr char kShowReasonHistogram[] = "Ash.Auth.ActiveSessionShowReason";
constexpr char kAuthStartedHistogram[] = "Ash.Auth.ActiveSessionAuthStart";
constexpr char kAuthFailedHistogram[] = "Ash.Auth.ActiveSessionAuthFailed";
constexpr char kAuthSucceededHistogram[] =
    "Ash.Auth.ActiveSessionAuthSucceeded";
constexpr char kClosedWithSuccessHistogram[] =
    "Ash.Auth.ActiveSessionAuthClosedWithSuccess";
constexpr char kClosedDuringAuthHistogram[] =
    "Ash.Auth.ActiveSessionAuthClosedDuringAuth";
constexpr char kOpenDurationHistogram[] =
    "Ash.Auth.ActiveSessionAuthOpenDuration";
constexpr char kNumberOfPinAttemptHistogram[] =
    "Ash.Auth.ActiveSessionAuthPinAttempt";
constexpr char kNumberOfPasswordAttemptHistogram[] =
    "Ash.Auth.ActiveSessionAuthPasswordAttempt";
constexpr char kNumberOfFingerprintAttemptHistogram[] =
    "Ash.Auth.ActiveSessionAuthFingerprintAttempt";

class ActiveSessionAuthMetricsRecorderTest : public AshTestBase {
 public:
  ActiveSessionAuthMetricsRecorderTest();

  ActiveSessionAuthMetricsRecorderTest(
      const ActiveSessionAuthMetricsRecorderTest&) = delete;
  ActiveSessionAuthMetricsRecorderTest& operator=(
      const ActiveSessionAuthMetricsRecorderTest&) = delete;

  ~ActiveSessionAuthMetricsRecorderTest() override;

  // AshTestBase:
  void SetUp() override;

 protected:
  // The test target.
  ActiveSessionAuthMetricsRecorder metrics_recorder_;

  // Used to verify recorded data.
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

ActiveSessionAuthMetricsRecorderTest::ActiveSessionAuthMetricsRecorderTest()
    : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

ActiveSessionAuthMetricsRecorderTest::~ActiveSessionAuthMetricsRecorderTest() =
    default;

void ActiveSessionAuthMetricsRecorderTest::SetUp() {
  AshTestBase::SetUp();
  histogram_tester_ = std::make_unique<base::HistogramTester>();
}

}  // namespace

// Verifies that histogram records the Password manager show reason.
TEST_F(ActiveSessionAuthMetricsRecorderTest, ShowReasonPasswordManagerTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kPasswordManager);
  histogram_tester_->ExpectUniqueSample(
      kShowReasonHistogram, AuthRequest::Reason::kPasswordManager, 1);
}

// Verifies that histogram records the settings show reason.
TEST_F(ActiveSessionAuthMetricsRecorderTest, ShowReasonSettingsTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kSettings);
  histogram_tester_->ExpectUniqueSample(kShowReasonHistogram,
                                        AuthRequest::Reason::kSettings, 1);
}

// Verifies that histogram records when password is submitted.
TEST_F(ActiveSessionAuthMetricsRecorderTest,
       ActiveSessionAuthStartWithPasswordTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kSettings);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPassword);
  histogram_tester_->ExpectUniqueSample(kAuthStartedHistogram,
                                        AuthInputType::kPassword, 1);
}

// Verifies that histogram records when PIN is submitted.
TEST_F(ActiveSessionAuthMetricsRecorderTest,
       ActiveSessionAuthStartWithPinTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kSettings);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPin);
  histogram_tester_->ExpectUniqueSample(kAuthStartedHistogram,
                                        AuthInputType::kPin, 1);
}

// Verifies that histogram records when password authentication is failed.
TEST_F(ActiveSessionAuthMetricsRecorderTest,
       ActiveSessionAuthPasswordFailedTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kPasswordManager);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPassword);
  metrics_recorder_.RecordAuthFailed(AuthInputType::kPassword);
  histogram_tester_->ExpectUniqueSample(kAuthFailedHistogram,
                                        AuthInputType::kPassword, 1);
}

// Verifies that histogram records when PIN authentication is failed.
TEST_F(ActiveSessionAuthMetricsRecorderTest, ActiveSessionAuthPinFailedTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kPasswordManager);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPin);
  metrics_recorder_.RecordAuthFailed(AuthInputType::kPin);
  histogram_tester_->ExpectUniqueSample(kAuthFailedHistogram,
                                        AuthInputType::kPin, 1);
}

// Verifies that histogram records when password authentication is succeeded.
TEST_F(ActiveSessionAuthMetricsRecorderTest,
       ActiveSessionAuthPasswordSucceededTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kPasswordManager);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPassword);
  metrics_recorder_.RecordAuthSucceeded(AuthInputType::kPassword);
  histogram_tester_->ExpectUniqueSample(kAuthSucceededHistogram,
                                        AuthInputType::kPassword, 1);
}

// Verifies that histogram records when PIN authentication is succeeded.
TEST_F(ActiveSessionAuthMetricsRecorderTest,
       ActiveSessionAuthPinSucceededTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kPasswordManager);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPin);
  metrics_recorder_.RecordAuthSucceeded(AuthInputType::kPin);
  histogram_tester_->ExpectUniqueSample(kAuthSucceededHistogram,
                                        AuthInputType::kPin, 1);
}

// Verifies that histogram records when closed after authentication is
// succeeded.
TEST_F(ActiveSessionAuthMetricsRecorderTest, ActiveSessionCloseSucceededTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kPasswordManager);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPassword);
  metrics_recorder_.RecordAuthSucceeded(AuthInputType::kPassword);
  histogram_tester_->ExpectUniqueSample(kClosedWithSuccessHistogram, true, 0);
  metrics_recorder_.RecordClose();
  histogram_tester_->ExpectUniqueSample(kClosedWithSuccessHistogram, true, 1);
}

// Verifies that histogram records when PIN authentication is succeeded.
TEST_F(ActiveSessionAuthMetricsRecorderTest, ActiveSessionCloseFailedTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kPasswordManager);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPin);
  metrics_recorder_.RecordAuthFailed(AuthInputType::kPin);
  histogram_tester_->ExpectUniqueSample(kClosedWithSuccessHistogram, false, 0);
  metrics_recorder_.RecordClose();
  histogram_tester_->ExpectUniqueSample(kClosedWithSuccessHistogram, false, 1);
}

// Verifies that histogram records when closed happens during the
// authentication.
TEST_F(ActiveSessionAuthMetricsRecorderTest,
       ActiveSessionClosedDuringAuthTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kSettings);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPassword);
  histogram_tester_->ExpectUniqueSample(kClosedDuringAuthHistogram, true, 0);
  metrics_recorder_.RecordClose();
  histogram_tester_->ExpectUniqueSample(kClosedDuringAuthHistogram, true, 1);
}

// Verifies that histogram records when closed happens after the authentication.
TEST_F(ActiveSessionAuthMetricsRecorderTest, ActiveSessionClosedAfterAuthTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kPasswordManager);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPin);
  metrics_recorder_.RecordAuthSucceeded(AuthInputType::kPin);
  histogram_tester_->ExpectUniqueSample(kClosedDuringAuthHistogram, false, 0);
  metrics_recorder_.RecordClose();
  histogram_tester_->ExpectUniqueSample(kClosedDuringAuthHistogram, false, 1);
}

// Verifies that histogram records that how long was the dialog shown.
TEST_F(ActiveSessionAuthMetricsRecorderTest, ActiveSessionOpenDurationTest) {
  const base::TimeDelta kShowDuration = base::Seconds(3);
  metrics_recorder_.RecordShow(AuthRequest::Reason::kPasswordManager);
  metrics_recorder_.RecordAuthStarted(AuthInputType::kPin);
  task_environment()->AdvanceClock(kShowDuration);
  metrics_recorder_.RecordClose();
  histogram_tester_->ExpectTimeBucketCount(kOpenDurationHistogram,
                                           kShowDuration, 1);
}

// Verifies that histogram records the password authentication attempt counter.
TEST_F(ActiveSessionAuthMetricsRecorderTest,
       ActiveSessionAuthPasswordAttemptTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kSettings);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPassword);
  metrics_recorder_.RecordAuthFailed(AuthInputType::kPassword);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPassword);
  metrics_recorder_.RecordAuthFailed(AuthInputType::kPassword);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPassword);
  metrics_recorder_.RecordAuthSucceeded(AuthInputType::kPassword);
  metrics_recorder_.RecordClose();
  histogram_tester_->ExpectBucketCount(kNumberOfPasswordAttemptHistogram, 3, 1);
}

// Verifies that histogram records the PIN authentication attempt counter.
TEST_F(ActiveSessionAuthMetricsRecorderTest, ActiveSessionAuthPinAttemptTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kPasswordManager);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPin);
  metrics_recorder_.RecordAuthFailed(AuthInputType::kPin);

  metrics_recorder_.RecordAuthStarted(AuthInputType::kPin);
  metrics_recorder_.RecordAuthSucceeded(AuthInputType::kPin);
  metrics_recorder_.RecordClose();
  histogram_tester_->ExpectBucketCount(kNumberOfPinAttemptHistogram, 2, 1);
}

// Verifies that histogram records the fingerprint authentication attempt
// counter.
TEST_F(ActiveSessionAuthMetricsRecorderTest,
       ActiveSessionAuthFingerprintAttemptTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kPasswordManager);

  metrics_recorder_.RecordAuthFailed(AuthInputType::kFingerprint);

  metrics_recorder_.RecordAuthSucceeded(AuthInputType::kFingerprint);
  metrics_recorder_.RecordClose();
  histogram_tester_->ExpectBucketCount(kNumberOfFingerprintAttemptHistogram, 2,
                                       1);
}

// Verifies that histogram records the fingerprint and authentication attempt
// counter.
TEST_F(ActiveSessionAuthMetricsRecorderTest,
       ActiveSessionAuthFingerprintAndPinAttemptTest) {
  metrics_recorder_.RecordShow(AuthRequest::Reason::kPasswordManager);

  metrics_recorder_.RecordAuthFailed(AuthInputType::kFingerprint);

  // Pin auhtentication started but concurrently the fingerprint succeeded
  metrics_recorder_.RecordAuthStarted(AuthInputType::kPin);
  metrics_recorder_.RecordAuthSucceeded(AuthInputType::kFingerprint);
  metrics_recorder_.RecordClose();

  histogram_tester_->ExpectBucketCount(kNumberOfPinAttemptHistogram, 1, 1);
  histogram_tester_->ExpectBucketCount(kNumberOfFingerprintAttemptHistogram, 2,
                                       1);
}

}  // namespace ash

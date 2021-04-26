// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/mock_signal_reporter.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/common/proto/device_trust_report_event.pb.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::A;
using testing::Invoke;

namespace {

const base::Value origins[]{base::Value("example1.example.com"),
                            base::Value("example2.example.com")};
const base::Value more_origins[]{base::Value("example1.example.com"),
                                 base::Value("example2.example.com"),
                                 base::Value("example3.example.com")};
}  // namespace

namespace enterprise_connectors {

class DeviceTrustServiceTest : public testing::Test {
  using Reporter = enterprise_connectors::MockDeviceTrustSignalReporter;

 public:
  DeviceTrustServiceTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    testing::Test::SetUp();
    OSCryptMocker::SetUp();
  }

  void TearDown() override {
    OSCryptMocker::TearDown();
    testing::Test::TearDown();
  }

  void ClearServicePolicy() {
    prefs()->RemoveUserPref(
        enterprise_connectors::kContextAwareAccessSignalsAllowlistPref);
  }

  void EnableServicePolicy() {
    prefs()->SetUserPref(
        enterprise_connectors::kContextAwareAccessSignalsAllowlistPref,
        std::make_unique<base::ListValue>(origins));
  }

  void UpdateServicePolicy() {
    prefs()->SetUserPref(
        enterprise_connectors::kContextAwareAccessSignalsAllowlistPref,
        std::make_unique<base::ListValue>(more_origins));
  }

  void DisableService() {
    prefs()->SetUserPref(
        enterprise_connectors::kContextAwareAccessSignalsAllowlistPref,
        std::make_unique<base::ListValue>());
  }

  void SetUpReporterForUpdates(DeviceTrustService* dt_service) {
    std::unique_ptr<Reporter> reporter = std::make_unique<Reporter>();

    // Initialize the reporter.
    Reporter* reporter_ptr = reporter.get();
    EXPECT_CALL(*reporter, Init(_, _))
        .WillOnce(Invoke([=](base::RepeatingCallback<bool()> policy_check,
                             DeviceTrustSignalReporter::Callback done_cb) {
          reporter_ptr->DeviceTrustSignalReporter::Init(policy_check,
                                                        std::move(done_cb));
        }));

    // Should only be sending protos.
    EXPECT_CALL(*reporter, SendReport(A<base::Value>(), _)).Times(0);

    // Mock that the proto has been sent and run the callback.
    EXPECT_CALL(*reporter, SendReport(A<const DeviceTrustReportEvent*>(), _))
        .WillRepeatedly(Invoke([=](const DeviceTrustReportEvent* report,
                                   Reporter::Callback sent_cb) {
          CheckReportAndRunCallback(dt_service, report, std::move(sent_cb));
        }));

    dt_service->SetSignalReporterForTesting(std::move(reporter));
    SetUp(dt_service);
  }

  void SetUpReporterInitializationFailure(DeviceTrustService* dt_service) {
    std::unique_ptr<Reporter> reporter = std::make_unique<Reporter>();

    // Mock the reporter initialization to fail.
    EXPECT_CALL(*reporter, Init(_, _))
        .WillOnce(Invoke([](base::RepeatingCallback<bool()> policy_check,
                            DeviceTrustSignalReporter::Callback done_cb) {
          std::move(done_cb).Run(false);
        }));

    dt_service->SetSignalReporterForTesting(std::move(reporter));
    SetUp(dt_service);
  }

  // Set up the Device Trust Service to listen for SignalReportCallback.
  // May be used multiple times to test failure code branches. However, in
  // production, since we needed to prevent the reporter from being initialized
  // and signal_report_callback_ is a base::OnceCallback, the callback is also
  // never re-set or called twice.
  void SetUp(DeviceTrustService* dt_service) {
    dt_service->SetSignalReportCallbackForTesting(base::BindOnce(
        &DeviceTrustServiceTest::ReportCallback, base::Unretained(this)));
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void WaitForSignalReported() { run_loop_->Run(); }

  void ReportCallback(bool success) {
    report_cb_ = true;
    report_sent_ = success;
    run_loop_->Quit();
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile_->GetTestingPrefService();
  }

  TestingProfile* profile() { return profile_.get(); }

  bool report_sent_ = false;
  bool report_cb_ = false;

 private:
  void CheckReportAndRunCallback(DeviceTrustService* dt_service,
                                 const DeviceTrustReportEvent* report,
                                 Reporter::Callback sent_cb) {
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
    ASSERT_TRUE(report->has_attestation_credential());
    ASSERT_TRUE(report->attestation_credential().has_credential());
    const auto& credential = report->attestation_credential();
    EXPECT_EQ(credential.credential(),
              dt_service->GetAttestationCredentialForTesting());
    EXPECT_EQ(
        credential.format(),
        DeviceTrustReportEvent::Credential::EC_NID_X9_62_PRIME256V1_PUBLIC_DER);
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
    std::move(sent_cb).Run(true);
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_ = std::make_unique<TestingProfile>();
  ScopedTestingLocalState local_state_;
};

TEST_F(DeviceTrustServiceTest, StartWithEnabledPolicy) {
  EnableServicePolicy();
  DeviceTrustService* device_trust_service =
      DeviceTrustFactory::GetForProfile(profile());
  EXPECT_TRUE(device_trust_service->IsEnabled());
  ASSERT_FALSE(report_sent_);
}

TEST_F(DeviceTrustServiceTest, StartWithDisabledPolicy) {
  DisableService();
  DeviceTrustService* device_trust_service =
      DeviceTrustFactory::GetForProfile(profile());
  ASSERT_FALSE(device_trust_service->IsEnabled());

  SetUpReporterForUpdates(device_trust_service);
  EnableServicePolicy();
  WaitForSignalReported();
  EXPECT_TRUE(device_trust_service->IsEnabled());
  ASSERT_TRUE(report_cb_);
  ASSERT_TRUE(report_sent_);
}

// According to Chrome policy_templates "do's / don'ts" (go/policies-dos-donts),
// for a list policy, Chrome should behave the same way under no policy set or
// empty list.
TEST_F(DeviceTrustServiceTest, StartWithNoPolicy) {
  ClearServicePolicy();
  DeviceTrustService* device_trust_service =
      DeviceTrustFactory::GetForProfile(profile());
  ASSERT_FALSE(device_trust_service->IsEnabled());

  SetUpReporterForUpdates(device_trust_service);
  EnableServicePolicy();
  WaitForSignalReported();
  EXPECT_TRUE(device_trust_service->IsEnabled());
  ASSERT_TRUE(report_cb_);
  ASSERT_TRUE(report_sent_);
}

TEST_F(DeviceTrustServiceTest, ReporterInitializationFailure) {
  ClearServicePolicy();
  DeviceTrustService* device_trust_service =
      DeviceTrustFactory::GetForProfile(profile());
  ASSERT_FALSE(device_trust_service->IsEnabled());

  SetUpReporterInitializationFailure(device_trust_service);
  EnableServicePolicy();
  WaitForSignalReported();
  EXPECT_TRUE(device_trust_service->IsEnabled());
  ASSERT_TRUE(report_cb_);
  ASSERT_FALSE(report_sent_);

  SetUp(device_trust_service);
  UpdateServicePolicy();
  WaitForSignalReported();
  ASSERT_TRUE(report_cb_);
  ASSERT_FALSE(report_sent_);
}

}  // namespace enterprise_connectors

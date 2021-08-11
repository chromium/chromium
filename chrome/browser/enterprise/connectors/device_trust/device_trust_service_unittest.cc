// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/mock_attestation_service.h"
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

using test::MockAttestationService;

class DeviceTrustServiceTest : public testing::Test {
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

 protected:
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

  void DisableServicePolicy() {
    prefs()->SetUserPref(
        enterprise_connectors::kContextAwareAccessSignalsAllowlistPref,
        std::make_unique<base::ListValue>());
  }

  std::unique_ptr<DeviceTrustService> CreateService() {
    std::unique_ptr<MockAttestationService> mock_attestation_service =
        std::make_unique<MockAttestationService>();
    mock_attestation_service_ = mock_attestation_service.get();

    std::unique_ptr<MockDeviceTrustSignalReporter> mock_reporter =
        std::make_unique<MockDeviceTrustSignalReporter>();
    mock_reporter_ = mock_reporter.get();

    run_loop_ = std::make_unique<base::RunLoop>();

    return std::make_unique<DeviceTrustService>(
        prefs(), std::move(mock_attestation_service), std::move(mock_reporter),
        base::BindOnce(&DeviceTrustServiceTest::ReportCallback,
                       base::Unretained(this)));
  }

  void SetUpReporterForUpdates() {
    ASSERT_TRUE(mock_reporter_);
    ASSERT_TRUE(mock_attestation_service_);

    EXPECT_CALL(*mock_reporter_, Init(_, _))
        .WillOnce(Invoke([=](base::RepeatingCallback<bool()> policy_check,
                             DeviceTrustSignalReporter::Callback done_cb) {
          mock_reporter_->DeviceTrustSignalReporter::Init(policy_check,
                                                          std::move(done_cb));
        }));

    // Should only be sending protos.
    EXPECT_CALL(*mock_reporter_, SendReport(A<base::Value>(), _)).Times(0);

    // Mock that the proto has been sent and run the callback.
    EXPECT_CALL(*mock_reporter_,
                SendReport(A<const DeviceTrustReportEvent*>(), _))
        .WillOnce(Invoke([=](const DeviceTrustReportEvent* report,
                             MockDeviceTrustSignalReporter::Callback sent_cb) {
          std::move(sent_cb).Run(true);
        }));

    EXPECT_CALL(*mock_attestation_service_,
                StampReport(A<DeviceTrustReportEvent&>()))
        .Times(1);
  }

  void SetUpReporterInitializationFailure() {
    // Mock the reporter initialization to fail.
    EXPECT_CALL(*mock_reporter_, Init(_, _))
        .WillOnce(Invoke([](base::RepeatingCallback<bool()> policy_check,
                            DeviceTrustSignalReporter::Callback done_cb) {
          std::move(done_cb).Run(false);
        }));
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
  std::unique_ptr<base::RunLoop> run_loop_;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_ = std::make_unique<TestingProfile>();
  MockAttestationService* mock_attestation_service_;
  MockDeviceTrustSignalReporter* mock_reporter_;
  absl::optional<bool> signal_report_callback_value_;
  ScopedTestingLocalState local_state_;
};

TEST_F(DeviceTrustServiceTest, StartWithEnabledPolicy) {
  EnableServicePolicy();
  auto device_trust_service = CreateService();
  EXPECT_TRUE(device_trust_service->IsEnabled());
  ASSERT_FALSE(report_sent_);
}

TEST_F(DeviceTrustServiceTest, StartWithDisabledPolicy) {
  DisableServicePolicy();
  auto device_trust_service = CreateService();
  ASSERT_FALSE(device_trust_service->IsEnabled());

  SetUpReporterForUpdates();
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
  auto device_trust_service = CreateService();
  ASSERT_FALSE(device_trust_service->IsEnabled());

  SetUpReporterForUpdates();
  EnableServicePolicy();
  WaitForSignalReported();
  EXPECT_TRUE(device_trust_service->IsEnabled());
  ASSERT_TRUE(report_cb_);
  ASSERT_TRUE(report_sent_);
}

TEST_F(DeviceTrustServiceTest, ReporterInitializationFailure) {
  ClearServicePolicy();
  auto device_trust_service = CreateService();
  ASSERT_FALSE(device_trust_service->IsEnabled());

  SetUpReporterInitializationFailure();
  EnableServicePolicy();
  WaitForSignalReported();
  EXPECT_TRUE(device_trust_service->IsEnabled());
  ASSERT_TRUE(report_cb_);
  ASSERT_FALSE(report_sent_);
}

}  // namespace enterprise_connectors

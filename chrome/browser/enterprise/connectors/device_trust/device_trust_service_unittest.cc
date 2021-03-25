// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/mock_signal_reporter.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
namespace {

const base::Value origins[]{base::Value("example1.example.com"),
                            base::Value("example2.example.com")};

}  // namespace

namespace enterprise_connectors {

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

  void ClearService() {
    prefs()->RemoveUserPref(
        enterprise_connectors::kContextAwareAccessSignalsAllowlistPref);
  }

  void EnableService() {
    prefs()->SetUserPref(
        enterprise_connectors::kContextAwareAccessSignalsAllowlistPref,
        std::make_unique<base::ListValue>(origins));
  }

  void DisableService() {
    prefs()->SetUserPref(
        enterprise_connectors::kContextAwareAccessSignalsAllowlistPref,
        std::make_unique<base::ListValue>());
  }

  void SetUpReporterForUpdates(DeviceTrustService* dt_service) {
    using Reporter = enterprise_connectors::MockDeviceTrustSignalReporter;
    std::unique_ptr<Reporter> reporter = std::make_unique<Reporter>();
    EXPECT_CALL(*reporter, SendReport(_, _))
        .WillRepeatedly(Invoke(
            [this](base::Value value, base::OnceCallback<void(bool)> sent_cb) {
              signal_posted_ = std::move(value);
              std::move(sent_cb).Run(true);
            }));
    dt_service->SetSignalReporterForTesting(std::move(reporter));
    dt_service->SetSignalReportCallbackForTesting(base::BindOnce(
        &DeviceTrustServiceTest::ReportCallback, base::Unretained(this)));
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void WaitForSignalReported() { run_loop_->Run(); }

  void ReportCallback(bool success) { run_loop_->Quit(); }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile_->GetTestingPrefService();
  }
  TestingProfile* profile() { return profile_.get(); }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  base::Value signal_posted_;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_ = std::make_unique<TestingProfile>();
  ScopedTestingLocalState local_state_;
};

TEST_F(DeviceTrustServiceTest, StartWithEnabledPolicy) {
  EnableService();
  DeviceTrustService* device_trust_service =
      DeviceTrustFactory::GetForProfile(profile());
  EXPECT_TRUE(device_trust_service->IsEnabled());
}

TEST_F(DeviceTrustServiceTest, StartWithDisabledPolicy) {
  DisableService();
  DeviceTrustService* device_trust_service =
      DeviceTrustFactory::GetForProfile(profile());
  ASSERT_FALSE(device_trust_service->IsEnabled());

  SetUpReporterForUpdates(device_trust_service);
  EnableService();
  WaitForSignalReported();
  EXPECT_TRUE(device_trust_service->IsEnabled());
}

// According to Chrome policy_templates "do's / don'ts" (go/policies-dos-donts),
// for a list policy, Chrome should behave the same way under no policy set or
// empty list.
TEST_F(DeviceTrustServiceTest, StartWithNoPolicy) {
  ClearService();
  DeviceTrustService* device_trust_service =
      DeviceTrustFactory::GetForProfile(profile());
  ASSERT_FALSE(device_trust_service->IsEnabled());

  SetUpReporterForUpdates(device_trust_service);
  EnableService();
  WaitForSignalReported();
  EXPECT_TRUE(device_trust_service->IsEnabled());
}

}  // namespace enterprise_connectors

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/potential_password_theft_signal_processor.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/extension_telemetry/password_reuse_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

using SignalInfo = ExtensionTelemetryReportRequest_SignalInfo;
using PotentialPasswordTheftInfo =
    ExtensionTelemetryReportRequest_SignalInfo_PotentialPasswordTheftInfo;
using PasswordReuseEventInfo =
    ExtensionTelemetryReportRequest_SignalInfo_PotentialPasswordTheftInfo_PasswordReuseInfo;
using RemoteHostData =
    ExtensionTelemetryReportRequest_SignalInfo_PotentialPasswordTheftInfo_RemoteHostData;
using RemoteHostContactedInfo =
    ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo_RemoteHostInfo;

using LoginReputationClientReusedPasswordAccountType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordAccountType;

constexpr const char* kExtensionId[] = {"crx-0", "crx-1"};
const char* host_urls[] = {"http://www.google.com", "http://www.youtube.com",
                           "http://www.giggle.com", "http://www.yutube.com"};
RemoteHostContactedInfo::ProtocolType kProtocolType =
    RemoteHostContactedInfo::HTTP_HTTPS;

class PotentialPasswordTheftSignalProcessorTest : public ::testing::Test {
 protected:
  PotentialPasswordTheftSignalProcessorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // Initialize password reuse event struct
    pw_reuse_event_0.matches_signin_password = false;
    pw_reuse_event_0.reused_password_account_type =
        GetReusedPasswordAccountType(ReusedPasswordAccountType::GMAIL, false);
    pw_reuse_event_0.matching_domains = std::vector<std::string>();
    pw_reuse_event_0.reused_password_hash = 123;
    pw_reuse_event_0.count = 1;

    pw_reuse_event_1.matches_signin_password = true;
    pw_reuse_event_1.reused_password_account_type =
        GetReusedPasswordAccountType(ReusedPasswordAccountType::GMAIL, true);
    pw_reuse_event_1.matching_domains =
        std::vector<std::string>({"www.google.com", "www.youtube.com"});
    pw_reuse_event_1.reused_password_hash = 456;
    pw_reuse_event_1.count = 1;
  }

  LoginReputationClientReusedPasswordAccountType GetReusedPasswordAccountType(
      LoginReputationClientReusedPasswordAccountType::AccountType account_type,
      bool is_account_syncing) {
    LoginReputationClientReusedPasswordAccountType reused_password_account_type;
    reused_password_account_type.set_account_type(account_type);
    reused_password_account_type.set_is_account_syncing(is_account_syncing);
    return reused_password_account_type;
  }

  base::test::ScopedFeatureList scoped_feature_list;
  PotentialPasswordTheftSignalProcessor processor_;
  content::BrowserTaskEnvironment task_environment_;

  // Password reuse event info structs for signal creation;
  PasswordReuseInfo pw_reuse_event_0, pw_reuse_event_1;
};

TEST_F(PotentialPasswordTheftSignalProcessorTest,
       EmptyProcessorMapWithNoDataStored) {
  EXPECT_FALSE(processor_.HasDataToReportForTest());
}

// This test processes password reuse signal first, and remote host contacted
// signal second within one second. There will be data staged for report.
TEST_F(PotentialPasswordTheftSignalProcessorTest, ProcessTwoSignalsInOrder) {
  auto pw_reuse_signal = PasswordReuseSignal(kExtensionId[0], pw_reuse_event_0);
  auto remote_host_signal = RemoteHostContactedSignal(
      kExtensionId[0], GURL(host_urls[0]), kProtocolType);
  scoped_feature_list.InitAndEnableFeature(
      kExtensionTelemetryPotentialPasswordTheft);
  processor_.ProcessSignal(pw_reuse_signal);

  EXPECT_FALSE(processor_.IsPasswordQueueEmptyForTest());
  EXPECT_TRUE(processor_.IsRemoteHostURLQueueEmptyForTest());
  // No data should be staged as the password reuse event is not old enough, and
  // no remote host signal info is reported.
  EXPECT_FALSE(processor_.HasDataToReportForTest());

  processor_.ProcessSignal(remote_host_signal);
  EXPECT_FALSE(processor_.IsPasswordQueueEmptyForTest());
  EXPECT_FALSE(processor_.IsRemoteHostURLQueueEmptyForTest());
  // No data should be staged for report since password and remote host signals
  // are not mature enough for reporting.
  EXPECT_FALSE(processor_.HasDataToReportForTest());

  // Forward 2 seconds and trigger the signal processor.
  task_environment_.FastForwardBy(base::Seconds(2));
  processor_.ProcessSignal(remote_host_signal);

  // There should be staged data for report.
  EXPECT_TRUE(processor_.HasDataToReportForTest());
  // Verify that there is signal info only for the correct extension id.
  EXPECT_TRUE(processor_.GetSignalInfoForReport(kExtensionId[0]));
  EXPECT_FALSE(processor_.GetSignalInfoForReport(kExtensionId[1]));
}

// This test processes remote host signal first, and password reuse signal
// second within 1 second. There will be no data staged for report.
TEST_F(PotentialPasswordTheftSignalProcessorTest,
       ProcessTwoSignalsInReverseOrder) {
  auto pw_reuse_signal = PasswordReuseSignal(kExtensionId[0], pw_reuse_event_0);
  auto remote_host_signal_0 = RemoteHostContactedSignal(
      kExtensionId[0], GURL(host_urls[0]), kProtocolType);
  auto remote_host_signal_1 = RemoteHostContactedSignal(
      kExtensionId[0], GURL(host_urls[1]), kProtocolType);
  scoped_feature_list.InitAndEnableFeature(
      kExtensionTelemetryPotentialPasswordTheft);
  processor_.ProcessSignal(remote_host_signal_0);
  task_environment_.FastForwardBy(base::Milliseconds(100));
  processor_.ProcessSignal(remote_host_signal_1);
  // Forward 100 milliseconds and trigger the signal processor.
  task_environment_.FastForwardBy(base::Milliseconds(100));
  processor_.ProcessSignal(pw_reuse_signal);
  // Forward 2 seconds and trigger the signal processor.
  task_environment_.FastForwardBy(base::Seconds(2));
  // Receives a new signal to trigger signal processing. Remote host queue
  // should be emptied.
  processor_.ProcessSignal(pw_reuse_signal);

  EXPECT_FALSE(processor_.IsPasswordQueueEmptyForTest());
  // Remote host queue should be empty even though remote host signals are
  // received because the processor clean up unqualified data.
  EXPECT_FALSE(processor_.IsRemoteHostURLQueueEmptyForTest());
  EXPECT_FALSE(processor_.HasDataToReportForTest());
}

// This test processes qualified signals and verifies the proto data.
TEST_F(PotentialPasswordTheftSignalProcessorTest, VerifyProtoData) {
  auto pw_reuse_signal_0 =
      PasswordReuseSignal(kExtensionId[0], pw_reuse_event_0);
  auto pw_reuse_signal_1 =
      PasswordReuseSignal(kExtensionId[0], pw_reuse_event_0);
  auto pw_reuse_signal_2 =
      PasswordReuseSignal(kExtensionId[0], pw_reuse_event_1);

  auto remote_host_signal_0 = RemoteHostContactedSignal(
      kExtensionId[0], GURL(host_urls[0]), kProtocolType);
  auto remote_host_signal_1 = RemoteHostContactedSignal(
      kExtensionId[0], GURL(host_urls[1]), kProtocolType);
  auto remote_host_signal_2 = RemoteHostContactedSignal(
      kExtensionId[0], GURL(host_urls[2]), kProtocolType);
  auto remote_host_signal_3 = RemoteHostContactedSignal(
      kExtensionId[0], GURL(host_urls[3]), kProtocolType);

  scoped_feature_list.InitAndEnableFeature(
      kExtensionTelemetryPotentialPasswordTheft);
  processor_.ProcessSignal(pw_reuse_signal_0);
  task_environment_.FastForwardBy(base::Milliseconds(50));
  processor_.ProcessSignal(pw_reuse_signal_1);
  task_environment_.FastForwardBy(base::Milliseconds(50));
  processor_.ProcessSignal(pw_reuse_signal_2);
  task_environment_.FastForwardBy(base::Milliseconds(50));

  processor_.ProcessSignal(remote_host_signal_0);
  task_environment_.FastForwardBy(base::Milliseconds(10));
  processor_.ProcessSignal(remote_host_signal_1);
  task_environment_.FastForwardBy(base::Milliseconds(10));
  processor_.ProcessSignal(remote_host_signal_2);
  task_environment_.FastForwardBy(base::Milliseconds(10));
  processor_.ProcessSignal(remote_host_signal_3);

  // Forward 2000ms to make all the existing signals old enough to report.
  task_environment_.FastForwardBy(base::Milliseconds(2000));
  // Trigger signal processing again.
  processor_.ProcessSignal(remote_host_signal_0);

  // Retrieve signal info for the first extension.
  std::unique_ptr<SignalInfo> extension_0_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  // Retrieve signal info for the second extension.
  std::unique_ptr<SignalInfo> extension_1_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[1]);
  ASSERT_NE(extension_0_signal_info, nullptr);
  ASSERT_EQ(extension_1_signal_info, nullptr);

  // Verify password reuse info.
  const PotentialPasswordTheftInfo& potential_password_theft_info =
      extension_0_signal_info->potential_password_theft_info();
  // 2 valid password reused signals should be reported.
  ASSERT_EQ(potential_password_theft_info.reused_password_infos_size(), 2);
  // 4 valid remote hosts signals should be reported.
  ASSERT_EQ(potential_password_theft_info.remote_hosts_data_size(), 4);

  // Verify detailed password reuse info.
  {
    // Verify first password reuse info.
    const PasswordReuseEventInfo& password_reuse_info_0 =
        potential_password_theft_info.reused_password_infos(0);
    // 2 password reuse entried were recorded.
    ASSERT_EQ(password_reuse_info_0.count(), static_cast<uint32_t>(2));
    ASSERT_EQ(password_reuse_info_0.is_chrome_signin_password(), false);
    ASSERT_EQ(password_reuse_info_0.domains_matching_password_size(), 0);

    // Verify second password reuse info.
    const PasswordReuseEventInfo& password_reuse_info_1 =
        potential_password_theft_info.reused_password_infos(1);
    ASSERT_EQ(password_reuse_info_1.count(), static_cast<uint32_t>(1));
    ASSERT_EQ(password_reuse_info_1.is_chrome_signin_password(), true);
    // List size should be 2 as there are 2 URLs in the reputable domain list.
    ASSERT_EQ(password_reuse_info_1.domains_matching_password_size(), 2);
    ASSERT_EQ(password_reuse_info_1.domains_matching_password(0),
              "www.google.com");
    ASSERT_EQ(password_reuse_info_1.domains_matching_password(1),
              "www.youtube.com");
  }

  // Verify detailed remote host contacted info.
  {
    // Verify third remote host contacted info.
    const RemoteHostData& remote_host_info_0 =
        potential_password_theft_info.remote_hosts_data(0);
    ASSERT_EQ(remote_host_info_0.remote_host_url(), "www.giggle.com");
    // The count is 3 because there are 3 password reuse event.
    ASSERT_EQ(remote_host_info_0.count(), static_cast<uint32_t>(3));

    // Verify second remote host contacted info.
    const RemoteHostData& remote_host_info_1 =
        potential_password_theft_info.remote_hosts_data(1);
    ASSERT_EQ(remote_host_info_1.remote_host_url(), "www.google.com");
    // Only one is counted as potential password theft info because
    // www.google.com is in the reputable list of one of the 2 password reuse
    // event.
    ASSERT_EQ(remote_host_info_1.count(), static_cast<uint32_t>(2));

    // Verify first remote host contacted info.
    const RemoteHostData& remote_host_info_2 =
        potential_password_theft_info.remote_hosts_data(2);
    ASSERT_EQ(remote_host_info_2.remote_host_url(), "www.youtube.com");
    // Only one is counted as potential password theft info because
    // www.youtube.com is in the reputable list of one of the 2 password reuse
    // event.
    ASSERT_EQ(remote_host_info_2.count(), static_cast<uint32_t>(2));

    // Verify fourth remote host contacted info.
    const RemoteHostData& remote_host_info_3 =
        potential_password_theft_info.remote_hosts_data(3);
    ASSERT_EQ(remote_host_info_3.remote_host_url(), "www.yutube.com");
    // The count is 2 because there are 2 password reuse event.
    ASSERT_EQ(remote_host_info_3.count(), static_cast<uint32_t>(3));
  }
}

}  // namespace

}  // namespace safe_browsing

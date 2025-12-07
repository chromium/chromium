// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/enterprise/connectors/reporting/crash_reporting_context.h"

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/mock_realtime_reporting_client.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Channel override is not supported on Android platform
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/scoped_channel_override.h"
#endif

using base::test::EqualsProto;
using ::testing::_;
using ::testing::ByMove;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Return;

namespace enterprise_connectors {

#if !BUILDFLAG(IS_CHROMEOS)

namespace {

void CreateCrashReport(crashpad::CrashReportDatabase* database,
                       crashpad::CrashReportDatabase::Report* report) {
  std::unique_ptr<crashpad::CrashReportDatabase::NewReport> new_report;
  ASSERT_EQ(database->PrepareNewCrashReport(&new_report),
            crashpad::CrashReportDatabase::kNoError);
  static constexpr char kTest[] = "test";
  ASSERT_TRUE(new_report->Writer()->Write(kTest, sizeof(kTest)));
  crashpad::UUID uuid;
  EXPECT_EQ(database->FinishedWritingCrashReport(std::move(new_report), &uuid),
            crashpad::CrashReportDatabase::kNoError);
  EXPECT_EQ(database->LookUpCrashReport(uuid, report),
            crashpad::CrashReportDatabase::kNoError);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_ANDROID)
// Duplicating the definition of these variables here to ensure that changes to
// those values in the source file are deliberate and caught by tests otherwise.
constexpr char kCrashpadPollingIntervalFlag[] = "crashpad-polling-interval";
constexpr int kDefaultCrashpadPollingIntervalSeconds = 3600;
#endif

}  // namespace

class CrashReportingContextTest : public testing::TestWithParam<bool> {
 public:
  CrashReportingContextTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    EXPECT_TRUE(profile_manager_.SetUp());

    if (use_proto_format()) {
      feature_list_.InitAndEnableFeature(
          policy::kUploadRealtimeReportingEventsUsingProto);
    } else {
      feature_list_.InitAndDisableFeature(
          policy::kUploadRealtimeReportingEventsUsingProto);
    }
  }

  bool use_proto_format() { return GetParam(); }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(CrashReportingContextTest, GetNewReportsFromDB) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());
  std::unique_ptr<crashpad::CrashReportDatabase> database =
      crashpad::CrashReportDatabase::InitializeWithoutCreating(
          database_dir.GetPath());
  crashpad::CrashReportDatabase::Report report;
  CreateCrashReport(database.get(), &report);
  std::vector<crashpad::CrashReportDatabase::Report> reports =
      GetNewReportsFromDatabase(report.creation_time + 1, database.get());
  EXPECT_EQ(reports.size(), 0u);
  reports = GetNewReportsFromDatabase(report.creation_time - 1, database.get());
  EXPECT_EQ(reports.size(), 1u);
}

TEST_P(CrashReportingContextTest, GetAndSetLatestCrashReportingTime) {
  time_t timestamp = base::Time::Now().ToTimeT();

  SetLatestCrashReportTime(g_browser_process->local_state(), timestamp);
  ASSERT_EQ(timestamp,
            GetLatestCrashReportTime(g_browser_process->local_state()));
}

TEST_P(CrashReportingContextTest, UploadToReportingServer) {
  EXPECT_EQ(static_cast<long>(0u),
            GetLatestCrashReportTime(g_browser_process->local_state()));

  time_t timestamp = base::Time::Now().ToTimeT();
  std::vector<crashpad::CrashReportDatabase::Report> reports;
  crashpad::CrashReportDatabase::Report report;
  report.creation_time = timestamp;
  report.id = "123";
  reports.push_back(report);

  TestingProfile* profile =
      profile_manager_.CreateTestingProfile("fake-profile");
  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("fake-token"));
  RealtimeReportingClientFactory::GetInstance()->SetTestingFactory(
      profile, base::BindRepeating(&test::MockRealtimeReportingClient::
                                       CreateMockRealtimeReportingClient));

  test::SetOnSecurityEventReporting(
      profile->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/std::set<std::string>(),
      /*enabled_opt_in_events=*/
      std::map<std::string, std::vector<std::string>>());

  test::MockRealtimeReportingClient* reporting_client =
      static_cast<test::MockRealtimeReportingClient*>(
          RealtimeReportingClientFactory::GetForProfile(profile));

  ::chrome::cros::reporting::proto::Event expected_event_proto;
  base::Value::Dict expected_event;

  if (use_proto_format()) {
    auto* browser_crash_event =
        expected_event_proto.mutable_browser_crash_event();
    browser_crash_event->set_channel(
        version_info::GetChannelString(chrome::GetChannel()));
    browser_crash_event->set_version(version_info::GetVersionNumber());
    browser_crash_event->set_report_id("123");
    browser_crash_event->set_platform(version_info::GetOSType());
    *expected_event_proto.mutable_time() =
        ToProtoTimestamp(base::Time::FromTimeT(timestamp));

    EXPECT_CALL(*reporting_client,
                ReportEvent(EqualsProto(expected_event_proto), _))
        .Times(1);
  } else {
    expected_event.Set("channel",
                       version_info::GetChannelString(chrome::GetChannel()));
    expected_event.Set("version", version_info::GetVersionNumber());
    expected_event.Set("reportId", "123");
    expected_event.Set("platform", version_info::GetOSType());

    EXPECT_CALL(
        *reporting_client,
        ReportPastEvent(kBrowserCrashEvent, _, Eq(ByRef(expected_event)),
                        base::Time::FromTimeT(timestamp)))
        .Times(1);
  }

  UploadToReportingServer(reporting_client->AsWeakPtrImpl(),
                          g_browser_process->local_state(), reports);
  EXPECT_EQ(timestamp,
            GetLatestCrashReportTime(g_browser_process->local_state()));
}

INSTANTIATE_TEST_SUITE_P(, CrashReportingContextTest, ::testing::Bool());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_ANDROID)

struct PollingIntervalParams {
  PollingIntervalParams(chrome::ScopedChannelOverride::Channel channel,
                        const std::string cmd_flag,
                        int expected_interval)
      : channel(channel),
        cmd_flag(cmd_flag),
        expected_interval(expected_interval) {}

  chrome::ScopedChannelOverride::Channel channel;
  std::string cmd_flag;
  int expected_interval;
};

class CrashpadPollingIntervalTest
    : public testing::TestWithParam<PollingIntervalParams> {};

TEST_P(CrashpadPollingIntervalTest, GetCrashpadPollingInterval) {
  chrome::ScopedChannelOverride scoped_channel(GetParam().channel);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(kCrashpadPollingIntervalFlag,
                                  GetParam().cmd_flag);
  EXPECT_EQ(GetCrashpadPollingInterval(),
            base::Seconds(GetParam().expected_interval));
}

INSTANTIATE_TEST_SUITE_P(
    CrashpadPollingIntervalTest,
    CrashpadPollingIntervalTest,
    testing::Values(
        PollingIntervalParams(chrome::ScopedChannelOverride::Channel::kBeta,
                              "-10",
                              kDefaultCrashpadPollingIntervalSeconds),
        PollingIntervalParams(chrome::ScopedChannelOverride::Channel::kBeta,
                              "10",
                              10),
        PollingIntervalParams(chrome::ScopedChannelOverride::Channel::kStable,
                              "10",
                              kDefaultCrashpadPollingIntervalSeconds)));

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_ANDROID)

#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace enterprise_connectors

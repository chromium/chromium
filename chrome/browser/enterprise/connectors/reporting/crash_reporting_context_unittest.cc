// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/enterprise/connectors/reporting/crash_reporting_context.h"

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/test/base/scoped_channel_override.h"
#endif

using ::testing::_;
using ::testing::ByMove;
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Duplicating the definition of these variables here to ensure that changes to
// those values in the source file are deliberate and caught by tests otherwise.
constexpr char kCrashpadPollingIntervalFlag[] = "crashpad-polling-interval";
constexpr int kDefaultCrashpadPollingIntervalSeconds = 3600;
#endif

}  // namespace

class MockRealtimeCrashReportingClient : public RealtimeReportingClient {
 public:
  explicit MockRealtimeCrashReportingClient(content::BrowserContext* context)
      : RealtimeReportingClient(context) {}
  MockRealtimeCrashReportingClient(const MockRealtimeCrashReportingClient&) =
      delete;
  MockRealtimeCrashReportingClient& operator=(
      const MockRealtimeCrashReportingClient&) = delete;

  std::optional<ReportingSettings> GetReportingSettings() override {
    return ReportingSettings();
  }

  MOCK_METHOD4(ReportPastEvent,
               void(const std::string&,
                    const ReportingSettings& settings,
                    base::Value::Dict event,
                    const base::Time& time));
};

std::unique_ptr<KeyedService> CreateMockRealtimeCrashReportingClient(
    content::BrowserContext* profile) {
  return std::make_unique<MockRealtimeCrashReportingClient>(profile);
}

class CrashReportingContextTest : public testing::Test {
 public:
  CrashReportingContextTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { EXPECT_TRUE(profile_manager_.SetUp()); }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

TEST_F(CrashReportingContextTest, GetNewReportsFromDB) {
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

TEST_F(CrashReportingContextTest, GetAndSetLatestCrashReportingTime) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterInt64Pref(kLatestCrashReportCreationTime, 0);
  time_t timestamp = base::Time::Now().ToTimeT();

  SetLatestCrashReportTime(&pref_service, timestamp);
  ASSERT_EQ(timestamp, GetLatestCrashReportTime(&pref_service));
}

TEST_F(CrashReportingContextTest, UploadToReportingServer) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterInt64Pref(kLatestCrashReportCreationTime, 0);
  EXPECT_EQ(0u, GetLatestCrashReportTime(&pref_service));

  time_t timestamp = base::Time::Now().ToTimeT();
  std::vector<crashpad::CrashReportDatabase::Report> reports;
  crashpad::CrashReportDatabase::Report report;
  report.creation_time = timestamp;
  reports.push_back(report);

  TestingProfile* profile =
      profile_manager_.CreateTestingProfile("fake-profile");
  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("fake-token"));
  RealtimeReportingClientFactory::GetInstance()->SetTestingFactory(
      profile, base::BindRepeating(&CreateMockRealtimeCrashReportingClient));
  MockRealtimeCrashReportingClient* reporting_client =
      static_cast<MockRealtimeCrashReportingClient*>(
          RealtimeReportingClientFactory::GetForProfile(profile));

  EXPECT_CALL(*reporting_client,
              ReportPastEvent(kBrowserCrashEvent, _, _,
                              base::Time::FromTimeT(timestamp)))
      .Times(1);
  UploadToReportingServer(reporting_client->GetWeakPtr(), &pref_service,
                          reports);
  EXPECT_EQ(timestamp, GetLatestCrashReportTime(&pref_service));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

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
  base::CommandLine* commandLine = base::CommandLine::ForCurrentProcess();
  commandLine->AppendSwitchASCII(kCrashpadPollingIntervalFlag,
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

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace enterprise_connectors

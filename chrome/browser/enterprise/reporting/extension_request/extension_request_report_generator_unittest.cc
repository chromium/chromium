// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_generator.h"

#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/extensions_workflow_events.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {
namespace {

constexpr int kTimeStamp = 42;
constexpr char kJustification[] = "Please, I need dark mode everywhere!";
constexpr char kProfileName[] = "profile-1";
constexpr char kExtensionId1[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionId2[] = "abcdefghijklmnopabcdefghijklmnpo";

constexpr char kAllowedExtensionSettings[] = R"({
  "abcdefghijklmnopabcdefghijklmnop" : {
    "installation_mode": "allowed"
  }
})";

constexpr char kBlockedExtensionSettings[] = R"({
  "abcdefghijklmnopabcdefghijklmnop" : {
    "installation_mode": "blocked"
  }
})";

}  // namespace

class ExtensionRequestReportGeneratorTest : public ::testing::Test {
 public:
  ExtensionRequestReportGeneratorTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  void SetExtensionRequestsList(const std::vector<std::string>& pendings,
                                const std::vector<std::string>& uploadeds,
                                TestingProfile* profile) {
    SetRequestPrefs(pendings, prefs::kCloudExtensionRequestIds,
                    extension_misc::kExtensionRequestTimestamp, profile);
    SetRequestPrefs(uploadeds, kCloudExtensionRequestUploadedIds,
                    "upload_timestamp", profile);
  }

  void SetExtensionSettings(const std::string& settings_string,
                            TestingProfile* profile) {
    std::optional<base::Value> settings =
        base::JSONReader::Read(settings_string);
    ASSERT_TRUE(settings.has_value());
    profile->GetTestingPrefService()->SetManagedPref(
        extensions::pref_names::kExtensionManagement,
        base::Value::ToUniquePtrValue(std::move(*settings)));
  }

  std::vector<std::unique_ptr<ExtensionsWorkflowEvent>> GenerateReports(
      Profile* profile) {
    return generator_.Generate(
        ExtensionRequestReportGenerator::ExtensionRequestData(profile));
  }

  TestingProfile* CreateProfile(const std::string& profile_name) {
    TestingProfile* profile =
        profile_manager_.CreateTestingProfile(profile_name);
    profile->GetTestingPrefService()->SetManagedPref(
        prefs::kCloudExtensionRequestEnabled,
        std::make_unique<base::Value>(true));
    return profile;
  }

  void VerifyReport(ExtensionsWorkflowEvent* actual_report,
                    const std::string& expected_id,
                    bool is_removed) {
    EXPECT_EQ(expected_id, actual_report->id());
    if (!is_removed) {
      EXPECT_EQ(kTimeStamp, actual_report->request_timestamp_millis());
      EXPECT_EQ(actual_report->justification(), kJustification);
    }
    EXPECT_EQ(is_removed, actual_report->removed());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    EXPECT_EQ(ExtensionsWorkflowEvent::CHROME_OS_USER,
              actual_report->client_type());
#else
    EXPECT_EQ(ExtensionsWorkflowEvent::BROWSER_DEVICE,
              actual_report->client_type());
    EXPECT_EQ(policy::GetMachineName(), actual_report->device_name());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

 private:
  void SetRequestPrefs(const std::vector<std::string>& ids,
                       const std::string& pref_name,
                       const std::string& timestamp_name,
                       TestingProfile* profile) {
    base::Value::Dict id_values;
    for (const auto& id : ids) {
      id_values.Set(
          id,
          base::Value::Dict()
              .Set(timestamp_name,
                   ::base::TimeToValue(
                       base::Time::FromMillisecondsSinceUnixEpoch(kTimeStamp)))
              .Set(extension_misc::kExtensionWorkflowJustification,
                   base::Value(kJustification)));
    }

    profile->GetTestingPrefService()->SetUserPref(pref_name,
                                                  std::move(id_values));
  }

  content::BrowserTaskEnvironment task_environment_;
  ExtensionRequestReportGenerator generator_;
  TestingProfileManager profile_manager_;
};

TEST_F(ExtensionRequestReportGeneratorTest, AddRequests) {
  auto* profile = CreateProfile(kProfileName);
  SetExtensionRequestsList({kExtensionId1, kExtensionId2}, {}, profile);

  auto reports = GenerateReports(profile);

  EXPECT_EQ(2u, reports.size());
  VerifyReport(reports[0].get(), kExtensionId1, /*is_removed=*/false);
  VerifyReport(reports[1].get(), kExtensionId2, /*is_removed=*/false);

  reports = GenerateReports(profile);

  EXPECT_EQ(0u, reports.size());
}

TEST_F(ExtensionRequestReportGeneratorTest, RemovalRequest) {
  auto* profile = CreateProfile(kProfileName);
  SetExtensionRequestsList({}, {kExtensionId1, kExtensionId2}, profile);

  auto reports = GenerateReports(profile);

  EXPECT_EQ(2u, reports.size());
  VerifyReport(reports[0].get(), kExtensionId1, /*is_removed=*/true);
  VerifyReport(reports[1].get(), kExtensionId2, /*is_removed=*/true);

  reports = GenerateReports(profile);

  EXPECT_EQ(0u, reports.size());
}

TEST_F(ExtensionRequestReportGeneratorTest, ApprovedRequest) {
  auto* profile = CreateProfile(kProfileName);
  SetExtensionRequestsList({kExtensionId1}, {}, profile);
  SetExtensionSettings(kAllowedExtensionSettings, profile);

  auto reports = GenerateReports(profile);

  EXPECT_EQ(0u, reports.size());
}

TEST_F(ExtensionRequestReportGeneratorTest, RejectedRequest) {
  auto* profile = CreateProfile(kProfileName);
  SetExtensionRequestsList({kExtensionId1}, {}, profile);
  SetExtensionSettings(kBlockedExtensionSettings, profile);

  auto reports = GenerateReports(profile);

  EXPECT_EQ(0u, reports.size());
}

}  // namespace enterprise_reporting

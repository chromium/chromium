// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_generator.h"

#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/extensions_workflow_events.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {
namespace {

constexpr int kTimeStamp = 42;
constexpr char kProfileName[] = "profile";
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
                                const std::vector<std::string>& uploadeds) {
    SetRequestPrefs(pendings, prefs::kCloudExtensionRequestIds,
                    extension_misc::kExtensionRequestTimestamp);
    SetRequestPrefs(uploadeds, kCloudExtensionRequestUploadedIds,
                    "upload_timestamp");
  }

  void SetExtensionSettings(const std::string& settings_string) {
    base::Optional<base::Value> settings =
        base::JSONReader::Read(settings_string);
    ASSERT_TRUE(settings.has_value());
    profile_->GetTestingPrefService()->SetManagedPref(
        extensions::pref_names::kExtensionManagement,
        base::Value::ToUniquePtrValue(std::move(*settings)));
  }

  std::vector<std::unique_ptr<ExtensionsWorkflowEvent>> GenerateReports() {
    return generator_.Generate(profile_);
  }

  void CreateProfile() {
    profile_ = profile_manager_.CreateTestingProfile(kProfileName);
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kCloudExtensionRequestEnabled,
        std::make_unique<base::Value>(true));
  }

  void VerifyReport(ExtensionsWorkflowEvent* actual_report,
                    const std::string& expected_id,
                    bool is_removed) {
    EXPECT_EQ(expected_id, actual_report->id());
    if (!is_removed)
      EXPECT_EQ(kTimeStamp, actual_report->request_timestamp_millis());
    EXPECT_EQ(is_removed, actual_report->removed());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    EXPECT_EQ(ExtensionsWorkflowEvent::CHROME_OS_USER,
              actual_report->client_type());
#else
    EXPECT_EQ(ExtensionsWorkflowEvent::BROWSER_DEVICE,
              actual_report->client_type());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

 private:
  void SetRequestPrefs(const std::vector<std::string>& ids,
                       const std::string& pref_name,
                       const std::string& timestamp_name) {
    std::unique_ptr<base::Value> id_values =
        std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
    for (const auto& id : ids) {
      base::Value request_data(base::Value::Type::DICTIONARY);
      request_data.SetKey(
          timestamp_name,
          ::util::TimeToValue(base::Time::FromJavaTime(kTimeStamp)));
      id_values->SetKey(id, std::move(request_data));
    }

    profile_->GetTestingPrefService()->SetUserPref(pref_name,
                                                   std::move(id_values));
  }

  content::BrowserTaskEnvironment task_environment_;
  ExtensionRequestReportGenerator generator_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
};

TEST_F(ExtensionRequestReportGeneratorTest, AddRequests) {
  CreateProfile();
  SetExtensionRequestsList({kExtensionId1, kExtensionId2}, {});

  auto reports = GenerateReports();

  EXPECT_EQ(2u, reports.size());
  VerifyReport(reports[0].get(), kExtensionId1, /*is_removed=*/false);
  VerifyReport(reports[1].get(), kExtensionId2, /*is_removed=*/false);

  reports = GenerateReports();

  EXPECT_EQ(0u, reports.size());
}

TEST_F(ExtensionRequestReportGeneratorTest, RemovalRequest) {
  CreateProfile();
  SetExtensionRequestsList({}, {kExtensionId1, kExtensionId2});

  auto reports = GenerateReports();

  EXPECT_EQ(2u, reports.size());
  VerifyReport(reports[0].get(), kExtensionId1, /*is_removed=*/true);
  VerifyReport(reports[1].get(), kExtensionId2, /*is_removed=*/true);

  reports = GenerateReports();

  EXPECT_EQ(0u, reports.size());
}

TEST_F(ExtensionRequestReportGeneratorTest, ApprovedRequest) {
  CreateProfile();
  SetExtensionRequestsList({kExtensionId1}, {});
  SetExtensionSettings(kAllowedExtensionSettings);

  auto reports = GenerateReports();

  EXPECT_EQ(0u, reports.size());
}

TEST_F(ExtensionRequestReportGeneratorTest, RejectedRequest) {
  CreateProfile();
  SetExtensionRequestsList({kExtensionId1}, {});
  SetExtensionSettings(kBlockedExtensionSettings);

  auto reports = GenerateReports();

  EXPECT_EQ(0u, reports.size());
}

}  // namespace enterprise_reporting

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/real_time_report_generator.h"

#include <set>
#include <string>
#include <vector>

#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_generator.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/common/proto/synced/extensions_workflow_events.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_android.h"
using PlatformReportingDelegateFactory =
    enterprise_reporting::ReportingDelegateFactoryAndroid;
#else
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
using PlatformReportingDelegateFactory =
    enterprise_reporting::ReportingDelegateFactoryDesktop;
#endif

namespace enterprise_reporting {

class RealTimeReportGeneratorTest : public ::testing::Test {
 public:
  RealTimeReportGeneratorTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  TestingProfileManager* profile_manager() { return &profile_manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

TEST_F(RealTimeReportGeneratorTest, ExtensionRequest) {
  const std::string extension_id = "abcdefghijklmnopabcdefghijklmnop";

  TestingProfile* profile = profile_manager()->CreateTestingProfile("profile");

  profile->GetTestingPrefService()->SetManagedPref(
      enterprise_reporting::kCloudExtensionRequestEnabled, base::Value(true));

  auto requests = base::DictValue().Set(extension_id, base::DictValue());
  profile->GetTestingPrefService()->SetUserPref(
      enterprise_reporting::kCloudExtensionRequestIds, std::move(requests));

  PlatformReportingDelegateFactory factory;
  RealTimeReportGenerator generator{&factory};

  std::vector<std::unique_ptr<google::protobuf::MessageLite>> reports =
      generator.Generate(
          RealTimeReportType::kExtensionRequest,
          ExtensionRequestReportGenerator::ExtensionRequestData(profile));
  EXPECT_EQ(1u, reports.size());

  EXPECT_EQ(extension_id,
            static_cast<ExtensionsWorkflowEvent*>(reports[0].get())->id());
}

TEST_F(RealTimeReportGeneratorTest, ExtensionRequest_PolicyDisabled) {
  const std::string extension_id = "abcdefghijklmnopabcdefghijklmnop";

  TestingProfile* profile =
      profile_manager()->CreateTestingProfile("profile_2");

  profile->GetTestingPrefService()->SetManagedPref(
      enterprise_reporting::kCloudExtensionRequestEnabled, base::Value(false));

  auto requests = base::DictValue().Set(extension_id, base::DictValue());
  profile->GetTestingPrefService()->SetUserPref(
      enterprise_reporting::kCloudExtensionRequestIds, std::move(requests));

  PlatformReportingDelegateFactory factory;
  RealTimeReportGenerator generator{&factory};

  std::vector<std::unique_ptr<google::protobuf::MessageLite>> reports =
      generator.Generate(
          RealTimeReportType::kExtensionRequest,
          ExtensionRequestReportGenerator::ExtensionRequestData(profile));
  EXPECT_TRUE(reports.empty());
}

TEST_F(RealTimeReportGeneratorTest, ExtensionRequest_NoPendingRequests) {
  TestingProfile* profile =
      profile_manager()->CreateTestingProfile("profile_3");

  profile->GetTestingPrefService()->SetManagedPref(
      enterprise_reporting::kCloudExtensionRequestEnabled, base::Value(true));

  profile->GetTestingPrefService()->SetUserPref(
      enterprise_reporting::kCloudExtensionRequestIds, base::DictValue());

  PlatformReportingDelegateFactory factory;
  RealTimeReportGenerator generator{&factory};

  std::vector<std::unique_ptr<google::protobuf::MessageLite>> reports =
      generator.Generate(
          RealTimeReportType::kExtensionRequest,
          ExtensionRequestReportGenerator::ExtensionRequestData(profile));
  EXPECT_TRUE(reports.empty());
}

TEST_F(RealTimeReportGeneratorTest, ExtensionRequest_MultiplePendingRequests) {
  const std::string extension_id_1 = "abcdefghijklmnopabcdefghijklmnop";
  const std::string extension_id_2 = "bcdefghijklmnopabcdefghijklmnopa";

  TestingProfile* profile =
      profile_manager()->CreateTestingProfile("profile_4");

  profile->GetTestingPrefService()->SetManagedPref(
      enterprise_reporting::kCloudExtensionRequestEnabled, base::Value(true));

  auto requests = base::DictValue()
                      .Set(extension_id_1, base::DictValue())
                      .Set(extension_id_2, base::DictValue());
  profile->GetTestingPrefService()->SetUserPref(
      enterprise_reporting::kCloudExtensionRequestIds, std::move(requests));

  PlatformReportingDelegateFactory factory;
  RealTimeReportGenerator generator{&factory};

  std::vector<std::unique_ptr<google::protobuf::MessageLite>> reports =
      generator.Generate(
          RealTimeReportType::kExtensionRequest,
          ExtensionRequestReportGenerator::ExtensionRequestData(profile));
  ASSERT_EQ(2u, reports.size());

  std::set<std::string> generated_ids;
  for (const auto& report : reports) {
    generated_ids.insert(
        static_cast<ExtensionsWorkflowEvent*>(report.get())->id());
  }
  EXPECT_TRUE(generated_ids.contains(extension_id_1));
  EXPECT_TRUE(generated_ids.contains(extension_id_2));
}

TEST_F(RealTimeReportGeneratorTest,
       ExtensionRequest_TimestampAndJustification) {
  const std::string extension_id = "abcdefghijklmnopabcdefghijklmnop";
  const int64_t kFakeTimestamp = 1620000000000;
  const std::string kFakeJustification = "Need this extension for work";

  TestingProfile* profile =
      profile_manager()->CreateTestingProfile("profile_5");

  profile->GetTestingPrefService()->SetManagedPref(
      enterprise_reporting::kCloudExtensionRequestEnabled, base::Value(true));

  auto request_data =
      base::DictValue()
          .Set(extension_misc::kExtensionRequestTimestamp,
               ::base::TimeToValue(
                   base::Time::FromMillisecondsSinceUnixEpoch(kFakeTimestamp)))
          .Set(extension_misc::kExtensionWorkflowJustification,
               base::Value(kFakeJustification));
  auto requests = base::DictValue().Set(extension_id, std::move(request_data));
  profile->GetTestingPrefService()->SetUserPref(
      enterprise_reporting::kCloudExtensionRequestIds, std::move(requests));

  PlatformReportingDelegateFactory factory;
  RealTimeReportGenerator generator{&factory};

  std::vector<std::unique_ptr<google::protobuf::MessageLite>> reports =
      generator.Generate(
          RealTimeReportType::kExtensionRequest,
          ExtensionRequestReportGenerator::ExtensionRequestData(profile));
  ASSERT_EQ(1u, reports.size());

  auto* event = static_cast<ExtensionsWorkflowEvent*>(reports[0].get());
  EXPECT_EQ(extension_id, event->id());
  EXPECT_EQ(kFakeTimestamp, event->request_timestamp_millis());
  EXPECT_EQ(kFakeJustification, event->justification());
}

}  // namespace enterprise_reporting

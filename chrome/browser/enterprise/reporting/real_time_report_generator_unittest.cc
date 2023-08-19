// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/real_time_report_generator.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/enterprise/reporting/real_time_report_generator_desktop.h"
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/extensions_workflow_events.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

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
      prefs::kCloudExtensionRequestEnabled, base::Value(true));

  auto requests = base::Value::Dict().Set(extension_id, base::Value::Dict());
  profile->GetTestingPrefService()->SetUserPref(
      prefs::kCloudExtensionRequestIds, std::move(requests));

  ReportingDelegateFactoryDesktop factory;
  RealTimeReportGenerator generator{&factory};

  std::vector<std::unique_ptr<google::protobuf::MessageLite>> reports =
      generator.Generate(
          RealTimeReportType::kExtensionRequest,
          ExtensionRequestReportGenerator::ExtensionRequestData(profile));
  EXPECT_EQ(1u, reports.size());

  EXPECT_EQ(extension_id,
            static_cast<ExtensionsWorkflowEvent*>(reports[0].get())->id());
}

}  // namespace enterprise_reporting

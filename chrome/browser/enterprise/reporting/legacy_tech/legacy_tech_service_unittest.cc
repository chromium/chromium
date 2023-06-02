// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_service.h"
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

namespace enterprise_reporting {

class LegacyTechServiceTest : public ::testing::Test {
 public:
  LegacyTechServiceTest() = default;
  ~LegacyTechServiceTest() override = default;

  void SetUp() override {
    LegacyTechServiceFactory::GetInstance()->SetReportTrigger(
        mock_trigger_.Get());
  }

  void SetPolicy(const std::vector<std::string>& urls) {
    base::Value::List policy;
    for (const auto& url : urls) {
      policy.Append(base::Value(url));
    }
    profile_.GetTestingPrefService()->SetManagedPref(
        kCloudLegacyTechReportAllowlist,
        std::make_unique<base::Value>(std::move(policy)));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingProfile profile_;
  ::testing::StrictMock<base::MockCallback<LegacyTechReportTrigger>>
      mock_trigger_;
};

TEST_F(LegacyTechServiceTest, Disabled) {
  EXPECT_CALL(mock_trigger_, Run(_)).Times(0);
  LegacyTechServiceFactory::GetForProfile(&profile_)->ReportEvent(
      "type", GURL("https://example.com"), "filename",
      /*line=*/10, /*column=*/42);
}

TEST_F(LegacyTechServiceTest, NoMatched) {
  EXPECT_CALL(mock_trigger_, Run(_)).Times(0);
  SetPolicy({"www.example.com"});
  LegacyTechServiceFactory::GetForProfile(&profile_)->ReportEvent(
      "type", GURL("https://example.com"), "filename",
      /*line=*/10, /*column=*/42);
}

TEST_F(LegacyTechServiceTest, MatchedAndUpload) {
  LegacyTechReportGenerator::LegacyTechData expected_data = {
      "type",        base::Time::Now(), GURL("https://example.com"),
      "example.com", "filename",        /*line=*/10,
      /*column=*/42};

  EXPECT_CALL(mock_trigger_, Run(_)).Times(1);
  SetPolicy({"example.com"});
  LegacyTechServiceFactory::GetForProfile(&profile_)->ReportEvent(
      "type", GURL("https://example.com"), "filename",
      /*line=*/10, /*column=*/42);
}

}  // namespace enterprise_reporting

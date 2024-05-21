// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_service.h"

#include <functional>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/legacy_tech_cookie_issue_details.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Eq;

namespace enterprise_reporting {

namespace {

constexpr char kType[] = "type";
constexpr char kUrl[] = "https://example.com";
constexpr char kFrameUrl[] = "https://subdomain.frame.com/something";
constexpr char kFileName[] = "filename";
constexpr uint64_t kLine = 10;
constexpr uint64_t kColumn = 42;

constexpr char kCookieIssueScriptUrl[] = "https://example.com/magic.js";
constexpr char kCookieName[] = "my-test-cookie";
constexpr char kCookieDomain[] = "cookie-domain.com";
constexpr char kCookiePath[] = "/path";
constexpr content::LegacyTechCookieIssueDetails::AccessOperation
    kCookieAccessOperation =
        content::LegacyTechCookieIssueDetails::AccessOperation::kRead;

}  // namespace

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
      kType, GURL(kUrl), GURL(kFrameUrl), kFileName, kLine, kColumn,
      std::nullopt);
}

TEST_F(LegacyTechServiceTest, NoMatched) {
  EXPECT_CALL(mock_trigger_, Run(_)).Times(0);
  SetPolicy({"www.example.com"});
  LegacyTechServiceFactory::GetForProfile(&profile_)->ReportEvent(
      kType, GURL(kUrl), GURL(kFrameUrl), kFileName, kLine, kColumn,
      std::nullopt);
}

TEST_F(LegacyTechServiceTest, MatchedAndUpload) {
  LegacyTechReportGenerator::LegacyTechData expected_data = {
      kType,
      GURL(kUrl),
      GURL(kFrameUrl),
      /*matched_url=*/"example.com",
      kFileName,
      kLine,
      kColumn,
      /*cookie_issue_details=*/std::nullopt};

  EXPECT_CALL(mock_trigger_, Run(Eq(std::ref(expected_data)))).Times(1);
  SetPolicy({"example.com"});
  LegacyTechServiceFactory::GetForProfile(&profile_)->ReportEvent(
      kType, GURL(kUrl), GURL(kFrameUrl), kFileName, kLine, kColumn,
      std::nullopt);
}

TEST_F(LegacyTechServiceTest, MatchedAndUploadWithCookieIssueDetails) {
  content::LegacyTechCookieIssueDetails cookie_issue_details = {
      GURL(kCookieIssueScriptUrl), kCookieName, kCookieDomain, kCookiePath,
      kCookieAccessOperation};
  LegacyTechReportGenerator::LegacyTechData expected_data = {
      kType,
      GURL(kUrl),
      GURL(kFrameUrl),
      /*matched_url=*/"example.com",
      kFileName,
      kLine,
      kColumn,
      cookie_issue_details};

  EXPECT_CALL(mock_trigger_, Run(Eq(std::ref(expected_data)))).Times(1);
  SetPolicy({"example.com"});
  LegacyTechServiceFactory::GetForProfile(&profile_)->ReportEvent(
      kType, GURL(kUrl), GURL(kFrameUrl), kFileName, kLine, kColumn,
      cookie_issue_details);
}

TEST_F(LegacyTechServiceTest, DelayedInitialization) {
  LegacyTechServiceFactory::GetInstance()->SetReportTrigger(
      base::RepeatingCallback<void(
          LegacyTechReportGenerator::LegacyTechData)>());
  EXPECT_CALL(mock_trigger_, Run(_)).Times(0);
  SetPolicy({"example.com"});
  LegacyTechServiceFactory::GetForProfile(&profile_)->ReportEvent(
      kType, GURL(kUrl), GURL(kFrameUrl), kFileName, kLine, kColumn,
      std::nullopt);
  ::testing::Mock::VerifyAndClearExpectations(&mock_trigger_);

  LegacyTechReportGenerator::LegacyTechData expected_data = {
      kType,
      GURL(kUrl),
      GURL(kFrameUrl),
      /*matched_url=*/"example.com",
      kFileName,
      kLine,
      kColumn,
      /*cookie_issue_details=*/std::nullopt};

  EXPECT_CALL(mock_trigger_, Run(Eq(std::ref(expected_data)))).Times(1);
  LegacyTechServiceFactory::GetInstance()->SetReportTrigger(
      mock_trigger_.Get());
}

TEST_F(LegacyTechServiceTest, MatchedAndUploadWithFrameUrl) {
  LegacyTechReportGenerator::LegacyTechData expected_data = {
      kType,
      GURL(kUrl),
      GURL(kFrameUrl),
      /*matched_url=*/"frame.com",
      kFileName,
      kLine,
      kColumn,
      /*cookie_issue_details=*/std::nullopt};

  EXPECT_CALL(mock_trigger_, Run(Eq(std::ref(expected_data)))).Times(1);
  SetPolicy({"frame.com"});
  LegacyTechServiceFactory::GetForProfile(&profile_)->ReportEvent(
      kType, GURL(kUrl), GURL(kFrameUrl), kFileName, kLine, kColumn,
      std::nullopt);
}

TEST_F(LegacyTechServiceTest, DelayedInitializationWithFrameUrl) {
  LegacyTechServiceFactory::GetInstance()->SetReportTrigger(
      base::RepeatingCallback<void(
          LegacyTechReportGenerator::LegacyTechData)>());
  EXPECT_CALL(mock_trigger_, Run(_)).Times(0);
  SetPolicy({"frame.com"});
  LegacyTechServiceFactory::GetForProfile(&profile_)->ReportEvent(
      kType, GURL(kUrl), GURL(kFrameUrl), kFileName, kLine, kColumn,
      std::nullopt);
  ::testing::Mock::VerifyAndClearExpectations(&mock_trigger_);

  LegacyTechReportGenerator::LegacyTechData expected_data = {
      kType,
      GURL(kUrl),
      GURL(kFrameUrl),
      /*matched_url=*/"frame.com",
      kFileName,
      kLine,
      kColumn,
      /*cookie_issue_details=*/std::nullopt};

  EXPECT_CALL(mock_trigger_, Run(Eq(std::ref(expected_data)))).Times(1);
  LegacyTechServiceFactory::GetInstance()->SetReportTrigger(
      mock_trigger_.Get());
}

}  // namespace enterprise_reporting

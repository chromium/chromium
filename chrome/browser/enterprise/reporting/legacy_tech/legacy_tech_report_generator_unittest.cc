// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"

#include <optional>

#include "base/time/time.h"
#include "components/enterprise/common/proto/legacy_tech_events.pb.h"
#include "content/public/browser/legacy_tech_cookie_issue_details.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_reporting {

namespace {

constexpr base::Time::Exploded kTestDate = {.year = 2023,
                                            .month = 5,
                                            .day_of_week = 4,
                                            .day_of_month = 4,
                                            .hour = 22,
                                            .minute = 10,
                                            .second = 15};

constexpr base::Time::Exploded kTestDateInMidnight = {.year = 2023,
                                                      .month = 5,
                                                      .day_of_week = 4,
                                                      .day_of_month = 4};

constexpr char kType[] = "type";
constexpr char kUrl[] = "https://www.example.com/path";
constexpr char kFrameUrl[] = "https://www.frame.com/something";
constexpr char kMatchedUrl[] = "www.example.com";
constexpr char kFileName[] = "filename.js";
constexpr uint64_t kLine = 10;
constexpr uint64_t kColumn = 42;

constexpr char kCookieTransferOrScriptUrl[] = "script url";
constexpr char kCookieName[] = "cookie name";
constexpr char kCookieDomain[] = "cookie domain";
constexpr char kCookiePath[] = "cookie path";

}  // namespace

class LegacyTechGeneratorTest : public ::testing::Test {
 public:
  LegacyTechGeneratorTest() = default;
  ~LegacyTechGeneratorTest() override = default;
};

TEST_F(LegacyTechGeneratorTest, Test) {
  LegacyTechReportGenerator::LegacyTechData data = {
      /*type=*/kType,
      /*timestamp=*/base::Time(),
      /*url=*/GURL(kUrl),
      /*frame_url=*/GURL(kFrameUrl),
      /*matched_url=*/kMatchedUrl,
      /*filename=*/kFileName,
      /*line=*/kLine,
      /*column=*/kColumn,
      /*cookie_issue_details=*/std::nullopt};
  ASSERT_TRUE(base::Time::FromUTCExploded(kTestDate, &data.timestamp));

  LegacyTechReportGenerator generator;
  std::unique_ptr<LegacyTechEvent> report = generator.Generate(data);

  EXPECT_EQ(kType, report->feature_id());
  EXPECT_EQ(kUrl, report->url());
  EXPECT_EQ(kFrameUrl, report->frame_url());
  EXPECT_EQ(kMatchedUrl, report->allowlisted_url_match());
  EXPECT_EQ(kFileName, report->filename());
  EXPECT_EQ(kColumn, report->column());
  EXPECT_EQ(kLine, report->line());

  EXPECT_FALSE(report->has_cookie_issue_details());

  base::Time midnight;
  ASSERT_TRUE(base::Time::FromUTCExploded(kTestDateInMidnight, &midnight));
  EXPECT_EQ(midnight.InMillisecondsSinceUnixEpoch(),
            report->event_timestamp_millis());
}

TEST_F(LegacyTechGeneratorTest, TestWithCookieIssueDetailsRead) {
  content::LegacyTechCookieIssueDetails cookie_issue_details = {
      kCookieTransferOrScriptUrl,
      kCookieName,
      kCookieDomain,
      kCookiePath,
      content::LegacyTechCookieIssueDetails::AccessOperation::kRead,
  };

  LegacyTechReportGenerator::LegacyTechData data = {
      /*type=*/kType,
      /*timestamp=*/base::Time(),
      /*url=*/GURL(kUrl),
      /*frame_url=*/GURL(kFrameUrl),
      /*matched_url=*/kMatchedUrl,
      /*filename=*/kFileName,
      /*line=*/kLine,
      /*column=*/kColumn,
      /*cookie_issue_details=*/
      std::move(cookie_issue_details)};
  ASSERT_TRUE(base::Time::FromUTCExploded(kTestDate, &data.timestamp));

  LegacyTechReportGenerator generator;
  std::unique_ptr<LegacyTechEvent> report = generator.Generate(data);

  EXPECT_TRUE(report->has_cookie_issue_details());
  EXPECT_EQ(kCookieTransferOrScriptUrl,
            report->cookie_issue_details().transfer_or_script_url());
  EXPECT_EQ(kCookieName, report->cookie_issue_details().name());
  EXPECT_EQ(kCookieDomain, report->cookie_issue_details().domain());
  EXPECT_EQ(kCookiePath, report->cookie_issue_details().path());
  EXPECT_EQ(CookieAccessOperation::COOKIE_ACCESS_OPERATION_READ,
            report->cookie_issue_details().access_operation());
}

TEST_F(LegacyTechGeneratorTest, TestWithCookieIssueDetailsWrite) {
  content::LegacyTechCookieIssueDetails cookie_issue_details = {
      kCookieTransferOrScriptUrl,
      kCookieName,
      kCookieDomain,
      kCookiePath,
      content::LegacyTechCookieIssueDetails::AccessOperation::kWrite,
  };

  LegacyTechReportGenerator::LegacyTechData data = {
      /*type=*/kType,
      /*timestamp=*/base::Time(),
      /*url=*/GURL(kUrl),
      /*frame_url=*/GURL(kFrameUrl),
      /*matched_url=*/kMatchedUrl,
      /*filename=*/kFileName,
      /*line=*/kLine,
      /*column=*/kColumn,
      /*cookie_issue_details=*/
      std::move(cookie_issue_details)};
  ASSERT_TRUE(base::Time::FromUTCExploded(kTestDate, &data.timestamp));

  LegacyTechReportGenerator generator;
  std::unique_ptr<LegacyTechEvent> report = generator.Generate(data);

  EXPECT_EQ(CookieAccessOperation::COOKIE_ACCESS_OPERATION_WRITE,
            report->cookie_issue_details().access_operation());
}

}  // namespace enterprise_reporting

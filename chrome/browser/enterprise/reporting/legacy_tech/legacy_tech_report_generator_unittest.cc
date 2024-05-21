// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"

#include <optional>

#include "base/logging.h"
#include "components/enterprise/common/proto/legacy_tech_events.pb.h"
#include "content/public/browser/legacy_tech_cookie_issue_details.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_reporting {

namespace {

constexpr char kType[] = "type";
constexpr char kUrl[] = "https://www.example.com/path";
constexpr char kFrameUrl[] = "https://www.frame.com/something";
constexpr char kMatchedUrl[] = "www.example.com";
constexpr char kFileName[] = "filename.js";
constexpr uint64_t kLine = 10;
constexpr uint64_t kColumn = 42;

constexpr char kCookieTransferOrScriptUrl[] = "https://example.com/magic.js";
constexpr char kLongCookieTransferOrScriptUrl[] =
    "https://username:password@example.com:8080/path/script.js?key=value#ref";
constexpr char kCookieName[] = "cookie name";
constexpr char kCookieDomain[] = "cookie domain";
constexpr char kCookiePath[] = "cookie path";

constexpr char kLongUrl[] =
    "https://username:password@example.com:8080/path/file.html?key=value#ref";

}  // namespace

class LegacyTechGeneratorTest : public ::testing::Test {
 public:
  LegacyTechGeneratorTest() = default;
  ~LegacyTechGeneratorTest() override = default;
};

TEST_F(LegacyTechGeneratorTest, Test) {
  LegacyTechReportGenerator::LegacyTechData data = {
      /*type=*/kType,
      /*url=*/GURL(kUrl),
      /*frame_url=*/GURL(kFrameUrl),
      /*matched_url=*/kMatchedUrl,
      /*filename=*/kFileName,
      /*line=*/kLine,
      /*column=*/kColumn,
      /*cookie_issue_details=*/std::nullopt};

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
}

TEST_F(LegacyTechGeneratorTest, TestWithCookieIssueDetailsRead) {
  content::LegacyTechCookieIssueDetails cookie_issue_details = {
      GURL(kCookieTransferOrScriptUrl),
      kCookieName,
      kCookieDomain,
      kCookiePath,
      content::LegacyTechCookieIssueDetails::AccessOperation::kRead,
  };

  LegacyTechReportGenerator::LegacyTechData data = {
      /*type=*/kType,
      /*url=*/GURL(kUrl),
      /*frame_url=*/GURL(kFrameUrl),
      /*matched_url=*/kMatchedUrl,
      /*filename=*/kFileName,
      /*line=*/kLine,
      /*column=*/kColumn,
      /*cookie_issue_details=*/
      std::move(cookie_issue_details)};

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

TEST_F(LegacyTechGeneratorTest, TestDropScriptUrlDetails) {
  content::LegacyTechCookieIssueDetails cookie_issue_details = {
      GURL(kLongCookieTransferOrScriptUrl),
      kCookieName,
      kCookieDomain,
      kCookiePath,
      content::LegacyTechCookieIssueDetails::AccessOperation::kRead,
  };

  LegacyTechReportGenerator::LegacyTechData data = {
      /*type=*/kType,
      /*url=*/GURL(kUrl),
      /*frame_url=*/GURL(kFrameUrl),
      /*matched_url=*/kMatchedUrl,
      /*filename=*/kFileName,
      /*line=*/kLine,
      /*column=*/kColumn,
      /*cookie_issue_details=*/
      std::move(cookie_issue_details)};

  LegacyTechReportGenerator generator;
  std::unique_ptr<LegacyTechEvent> report = generator.Generate(data);

  std::string expected_url = "https://example.com:8080/path/script.js";

  EXPECT_TRUE(report->has_cookie_issue_details());
  EXPECT_EQ(expected_url,
            report->cookie_issue_details().transfer_or_script_url());
}

TEST_F(LegacyTechGeneratorTest, TestWithCookieIssueDetailsWrite) {
  content::LegacyTechCookieIssueDetails cookie_issue_details = {
      GURL(kCookieTransferOrScriptUrl),
      kCookieName,
      kCookieDomain,
      kCookiePath,
      content::LegacyTechCookieIssueDetails::AccessOperation::kWrite,
  };

  LegacyTechReportGenerator::LegacyTechData data = {
      /*type=*/kType,
      /*url=*/GURL(kUrl),
      /*frame_url=*/GURL(kFrameUrl),
      /*matched_url=*/kMatchedUrl,
      /*filename=*/kFileName,
      /*line=*/kLine,
      /*column=*/kColumn,
      /*cookie_issue_details=*/std::move(cookie_issue_details)};

  LegacyTechReportGenerator generator;
  std::unique_ptr<LegacyTechEvent> report = generator.Generate(data);

  EXPECT_EQ(CookieAccessOperation::COOKIE_ACCESS_OPERATION_WRITE,
            report->cookie_issue_details().access_operation());
}

TEST_F(LegacyTechGeneratorTest, TestDropUrlComponents) {
  LegacyTechReportGenerator::LegacyTechData data = {
      /*type=*/kType,
      /*url=*/GURL(kLongUrl),
      /*frame_url=*/GURL(kLongUrl),
      /*matched_url=*/kMatchedUrl,
      /*filename=*/kFileName,
      /*line=*/kLine,
      /*column=*/kColumn,
      /*cookie_issue_details=*/std::nullopt};

  LegacyTechReportGenerator generator;
  std::unique_ptr<LegacyTechEvent> report = generator.Generate(data);

  std::string expected_url = "https://example.com:8080/path/file.html";

  EXPECT_EQ(expected_url, report->url());
  EXPECT_EQ(expected_url, report->frame_url());
}

}  // namespace enterprise_reporting

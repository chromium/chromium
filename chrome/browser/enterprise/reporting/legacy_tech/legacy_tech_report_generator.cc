// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"
#include "components/enterprise/common/proto/legacy_tech_events.pb.h"

namespace enterprise_reporting {

LegacyTechReportGenerator::LegacyTechCookieIssueDetails::
    LegacyTechCookieIssueDetails() = default;
LegacyTechReportGenerator::LegacyTechCookieIssueDetails::
    LegacyTechCookieIssueDetails(const std::string& transfer_or_script_url,
                                 const std::string& name,
                                 const std::string& domain,
                                 const std::string& path,
                                 AccessOperation access_operation)
    : transfer_or_script_url(transfer_or_script_url),
      name(name),
      domain(domain),
      path(path),
      access_operation(access_operation) {}

LegacyTechReportGenerator::LegacyTechCookieIssueDetails::
    LegacyTechCookieIssueDetails(LegacyTechCookieIssueDetails&& other) =
        default;
LegacyTechReportGenerator::LegacyTechCookieIssueDetails&
LegacyTechReportGenerator::LegacyTechCookieIssueDetails::operator=(
    LegacyTechCookieIssueDetails&& other) = default;
LegacyTechReportGenerator::LegacyTechCookieIssueDetails::
    ~LegacyTechCookieIssueDetails() = default;

bool LegacyTechReportGenerator::LegacyTechCookieIssueDetails::operator==(
    const LegacyTechCookieIssueDetails& other) const = default;

LegacyTechReportGenerator::LegacyTechData::LegacyTechData() = default;
LegacyTechReportGenerator::LegacyTechData::LegacyTechData(
    const std::string& type,
    const base::Time& timestamp,
    const GURL& url,
    const std::string& matched_url,
    const std::string& filename,
    uint64_t line,
    uint64_t column,
    std::optional<LegacyTechCookieIssueDetails> cookie_issue_details)
    : type(type),
      timestamp(timestamp),
      url(url),
      matched_url(matched_url),
      filename(filename),
      line(line),
      column(column),
      cookie_issue_details(std::move(cookie_issue_details)) {}

LegacyTechReportGenerator::LegacyTechData::LegacyTechData(
    LegacyTechData&& other) = default;
LegacyTechReportGenerator::LegacyTechData&
LegacyTechReportGenerator::LegacyTechData::operator=(LegacyTechData&& other) =
    default;
LegacyTechReportGenerator::LegacyTechData::~LegacyTechData() = default;

bool LegacyTechReportGenerator::LegacyTechData::operator==(
    const LegacyTechData& other) const = default;

LegacyTechReportGenerator::LegacyTechReportGenerator() = default;
LegacyTechReportGenerator::~LegacyTechReportGenerator() = default;

std::unique_ptr<LegacyTechEvent> LegacyTechReportGenerator::Generate(
    const RealTimeReportGenerator::Data& data) {
  const LegacyTechData& legacy_tech_data =
      static_cast<const LegacyTechData&>(data);
  std::unique_ptr<LegacyTechEvent> report = std::make_unique<LegacyTechEvent>();
  report->set_feature_id(legacy_tech_data.type);
  // Blur timestamp for privacy.
  report->set_event_timestamp_millis(
      legacy_tech_data.timestamp.UTCMidnight().InMillisecondsSinceUnixEpoch());
  report->set_url(legacy_tech_data.url.spec());
  report->set_allowlisted_url_match(legacy_tech_data.matched_url);
  report->set_filename(legacy_tech_data.filename);
  report->set_column(legacy_tech_data.column);
  report->set_line(legacy_tech_data.line);

  if (legacy_tech_data.cookie_issue_details) {
    const LegacyTechCookieIssueDetails& cookie_issue_data =
        *legacy_tech_data.cookie_issue_details;
    CookieIssueDetails* cookie_issue_report =
        report->mutable_cookie_issue_details();

    cookie_issue_report->set_transfer_or_script_url(
        cookie_issue_data.transfer_or_script_url);
    cookie_issue_report->set_name(cookie_issue_data.name);
    cookie_issue_report->set_domain(cookie_issue_data.domain);
    cookie_issue_report->set_path(cookie_issue_data.path);

    switch (cookie_issue_data.access_operation) {
      case LegacyTechCookieIssueDetails::AccessOperation::kRead:
        cookie_issue_report->set_access_operation(
            CookieAccessOperation::COOKIE_ACCESS_OPERATION_READ);
        break;
      case LegacyTechCookieIssueDetails::AccessOperation::kWrite:
        cookie_issue_report->set_access_operation(
            CookieAccessOperation::COOKIE_ACCESS_OPERATION_WRITE);
        break;
    }
  }

  return report;
}

}  // namespace enterprise_reporting

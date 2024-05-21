// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"
#include "components/enterprise/common/proto/legacy_tech_events.pb.h"
#include "content/public/browser/legacy_tech_cookie_issue_details.h"

namespace enterprise_reporting {

namespace {
GURL SanitizeUrl(const GURL& url) {
  if (!url.is_valid()) {
    return GURL();
  }

  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  return url.ReplaceComponents(replacements);
}
}  // namespace

LegacyTechReportGenerator::LegacyTechData::LegacyTechData() = default;
LegacyTechReportGenerator::LegacyTechData::LegacyTechData(
    const std::string& type,
    const GURL& url,
    const GURL& frame_url,
    const std::string& matched_url,
    const std::string& filename,
    uint64_t line,
    uint64_t column,
    std::optional<content::LegacyTechCookieIssueDetails> cookie_issue_details)
    : type(type),
      url(url),
      frame_url(frame_url),
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
  report->set_url(SanitizeUrl(legacy_tech_data.url).spec());
  report->set_frame_url(SanitizeUrl(legacy_tech_data.frame_url).spec());
  report->set_allowlisted_url_match(legacy_tech_data.matched_url);
  report->set_filename(legacy_tech_data.filename);
  report->set_column(legacy_tech_data.column);
  report->set_line(legacy_tech_data.line);

  if (legacy_tech_data.cookie_issue_details) {
    const content::LegacyTechCookieIssueDetails& cookie_issue_data =
        *legacy_tech_data.cookie_issue_details;
    CookieIssueDetails* cookie_issue_report =
        report->mutable_cookie_issue_details();

    cookie_issue_report->set_transfer_or_script_url(
        SanitizeUrl(cookie_issue_data.transfer_or_script_url).spec());
    cookie_issue_report->set_name(cookie_issue_data.name);
    cookie_issue_report->set_domain(cookie_issue_data.domain);
    cookie_issue_report->set_path(cookie_issue_data.path);

    switch (cookie_issue_data.access_operation) {
      case content::LegacyTechCookieIssueDetails::AccessOperation::kRead:
        cookie_issue_report->set_access_operation(
            CookieAccessOperation::COOKIE_ACCESS_OPERATION_READ);
        break;
      case content::LegacyTechCookieIssueDetails::AccessOperation::kWrite:
        cookie_issue_report->set_access_operation(
            CookieAccessOperation::COOKIE_ACCESS_OPERATION_WRITE);
        break;
    }
  }

  return report;
}

}  // namespace enterprise_reporting

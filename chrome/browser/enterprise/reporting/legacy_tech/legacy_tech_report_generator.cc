// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"

namespace enterprise_reporting {

LegacyTechReportGenerator::LegacyTechData::LegacyTechData() = default;
LegacyTechReportGenerator::LegacyTechData::LegacyTechData(
    const std::string& type,
    const base::Time& timestamp,
    const GURL& url,
    const std::string& matched_url,
    const std::string& filename,
    uint64_t line,
    uint64_t column)
    : type(type),
      timestamp(timestamp),
      url(url),
      matched_url(matched_url),
      filename(filename),
      line(line),
      column(column) {}

LegacyTechReportGenerator::LegacyTechData::LegacyTechData(
    const LegacyTechData& other) = default;
LegacyTechReportGenerator::LegacyTechData::~LegacyTechData() = default;

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
  return report;
}

}  // namespace enterprise_reporting

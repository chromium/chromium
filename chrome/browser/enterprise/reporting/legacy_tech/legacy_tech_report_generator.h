// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_LEGACY_TECH_LEGACY_TECH_REPORT_GENERATOR_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_LEGACY_TECH_LEGACY_TECH_REPORT_GENERATOR_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/enterprise/browser/reporting/real_time_report_generator.h"
#include "components/enterprise/common/proto/legacy_tech_events.pb.h"
#include "content/public/browser/legacy_tech_cookie_issue_details.h"
#include "url/gurl.h"

namespace enterprise_reporting {

class LegacyTechReportGenerator {
 public:
  struct LegacyTechData : public RealTimeReportGenerator::Data {
    LegacyTechData();
    LegacyTechData(const std::string& type,
                   const GURL& url,
                   const GURL& frame_url,
                   const std::string& matched_url,
                   const std::string& filename,
                   uint64_t line,
                   uint64_t column,
                   std::optional<content::LegacyTechCookieIssueDetails>
                       cookie_issue_details);
    LegacyTechData(const LegacyTechData&) = delete;
    LegacyTechData(LegacyTechData&& other);
    LegacyTechData& operator=(const LegacyTechData&) = delete;
    LegacyTechData& operator=(LegacyTechData&& other);
    ~LegacyTechData();

    bool operator==(const LegacyTechData& other) const;

    std::string type;
    GURL url;
    GURL frame_url;
    std::string matched_url;
    std::string filename;
    uint64_t line;
    uint64_t column;
    std::optional<content::LegacyTechCookieIssueDetails> cookie_issue_details;
  };

  LegacyTechReportGenerator();
  LegacyTechReportGenerator(const LegacyTechReportGenerator&) = delete;
  LegacyTechReportGenerator& operator=(const LegacyTechReportGenerator&) =
      delete;
  ~LegacyTechReportGenerator();

  std::unique_ptr<LegacyTechEvent> Generate(
      const RealTimeReportGenerator::Data& data);
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_LEGACY_TECH_LEGACY_TECH_REPORT_GENERATOR_H_

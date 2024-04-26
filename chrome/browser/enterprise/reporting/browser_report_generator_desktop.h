// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_DESKTOP_H_

#include "components/enterprise/browser/reporting/browser_report_generator.h"
#include "components/version_info/channel.h"

namespace enterprise_management {
class BrowserReport;
}  // namespace enterprise_management

namespace enterprise_reporting {

// Desktop implementation of platform-specific info fetching for Enterprise
// browser report generation.
// TODO(crbug.com/40703888): Move Chrome OS code to its own delegate
class BrowserReportGeneratorDesktop : public BrowserReportGenerator::Delegate {
 public:
  BrowserReportGeneratorDesktop();
  BrowserReportGeneratorDesktop(const BrowserReportGeneratorDesktop&) = delete;
  BrowserReportGeneratorDesktop& operator=(
      const BrowserReportGeneratorDesktop&) = delete;
  ~BrowserReportGeneratorDesktop() override;

  std::string GetExecutablePath() override;
  version_info::Channel GetChannel() override;
  std::vector<BrowserReportGenerator::ReportedProfileData> GetReportedProfiles()
      override;
  bool IsExtendedStableChannel() override;
  // Adds the auto-updated version to the given report instance.
  void GenerateBuildStateInfo(
      enterprise_management::BrowserReport* report) override;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_DESKTOP_H_

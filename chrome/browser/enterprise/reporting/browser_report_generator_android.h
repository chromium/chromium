// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_ANDROID_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_ANDROID_H_

#include <string>

#include "components/enterprise/browser/reporting/browser_report_generator.h"

namespace enterprise_management {
class BrowserReport;
}  // namespace enterprise_management

namespace enterprise_reporting {

// Android implementation of platform-specific info fetching for Enterprise
// browser report generation.
class BrowserReportGeneratorAndroid : public BrowserReportGenerator::Delegate {
 public:
  BrowserReportGeneratorAndroid();
  BrowserReportGeneratorAndroid(const BrowserReportGeneratorAndroid&) = delete;
  BrowserReportGeneratorAndroid& operator=(
      const BrowserReportGeneratorAndroid&) = delete;
  ~BrowserReportGeneratorAndroid() override;

  // BrowserReportGenerator::Delegate implementation
  std::string GetExecutablePath() override;
  version_info::Channel GetChannel() override;
  std::vector<BrowserReportGenerator::ReportedProfileData> GetReportedProfiles()
      override;
  bool IsExtendedStableChannel() override;
  void GenerateBuildStateInfo(
      enterprise_management::BrowserReport* report) override;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_ANDROID_H_

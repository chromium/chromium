// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_DESKTOP_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/browser/reporting/browser_report_generator.h"
#include "components/version_info/channel.h"
#include "content/public/common/webplugininfo.h"

namespace enterprise_management {
class BrowserReport;
}  // namespace enterprise_management

namespace enterprise_reporting {

// Desktop implementation of platform-specific info fetching for Enterprise
// browser report generation.
// TODO(crbug.com/1102047): Move Chrome OS code to its own delegate
class BrowserReportGeneratorDesktop : public BrowserReportGenerator::Delegate {
 public:
  using ReportCallback = base::OnceCallback<void(
      std::unique_ptr<enterprise_management::BrowserReport>)>;

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
  void GeneratePluginsIfNeeded(
      ReportCallback callback,
      std::unique_ptr<enterprise_management::BrowserReport> report) override;

  void OnPluginsReady(
      ReportCallback callback,
      std::unique_ptr<enterprise_management::BrowserReport> report,
      const std::vector<content::WebPluginInfo>& plugins);

 private:
  base::WeakPtrFactory<BrowserReportGeneratorDesktop> weak_ptr_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_DESKTOP_H_

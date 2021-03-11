// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_DESKTOP_H_

#include "components/enterprise/browser/reporting/browser_report_generator.h"

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/version_info/channel.h"
#include "content/public/common/webplugininfo.h"

namespace em = ::enterprise_management;

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
  bool IsExtendedStableChannel() override;
  // Adds the auto-updated version to the given report instance.
  void GenerateBuildStateInfo(em::BrowserReport* report) override;
  // Generates user profiles info in the given report instance.
  void GenerateProfileInfo(ReportType report_type,
                           em::BrowserReport* report) override;
  void GeneratePluginsIfNeeded(
      ReportCallback callback,
      std::unique_ptr<em::BrowserReport> report) override;

  void OnPluginsReady(ReportCallback callback,
                      std::unique_ptr<em::BrowserReport> report,
                      const std::vector<content::WebPluginInfo>& plugins);

 private:
  base::WeakPtrFactory<BrowserReportGeneratorDesktop> weak_ptr_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_DESKTOP_H_

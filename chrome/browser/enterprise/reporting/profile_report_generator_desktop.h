// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_DESKTOP_H_

#include "chrome/browser/enterprise/reporting/profile_report_generator_delegate_base.h"

namespace policy {
class MachineLevelUserCloudPolicyManager;
}

namespace enterprise_management {
class ChromeUserProfileInfo;
}

namespace enterprise_reporting {

/**
 * Desktop implementation of the profile reporting delegate.
 */
class ProfileReportGeneratorDesktop
    : public ProfileReportGeneratorDelegateBase {
 public:
  ProfileReportGeneratorDesktop();
  ProfileReportGeneratorDesktop(const ProfileReportGeneratorDesktop&) = delete;
  ProfileReportGeneratorDesktop& operator=(
      const ProfileReportGeneratorDesktop&) = delete;
  ~ProfileReportGeneratorDesktop() override;

  // ProfileReportGenerator::Delegate implementation.
  void GetExtensionInfo(
      enterprise_management::ChromeUserProfileInfo* report) override;
  void GetExtensionRequest(
      enterprise_management::ChromeUserProfileInfo* report) override;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_DESKTOP_H_

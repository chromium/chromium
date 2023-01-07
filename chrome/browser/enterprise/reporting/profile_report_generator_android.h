// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_ANDROID_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_ANDROID_H_

#include "chrome/browser/enterprise/reporting/profile_report_generator_delegate_base.h"

namespace enterprise_management {
class ChromeUserProfileInfo;
}

namespace policy {
class MachineLevelUserCloudPolicyManager;
}

namespace enterprise_reporting {

/**
 * Android implementation of the profile reporting delegate.
 */
class ProfileReportGeneratorAndroid
    : public ProfileReportGeneratorDelegateBase {
 public:
  ProfileReportGeneratorAndroid();
  ProfileReportGeneratorAndroid(const ProfileReportGeneratorAndroid&) = delete;
  ProfileReportGeneratorAndroid& operator=(
      const ProfileReportGeneratorAndroid&) = delete;
  ~ProfileReportGeneratorAndroid() override;

  // ProfileReportGenerator::Delegate implementation.
  void GetExtensionInfo(
      enterprise_management::ChromeUserProfileInfo* report) override;
  void GetExtensionRequest(
      enterprise_management::ChromeUserProfileInfo* report) override;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_ANDROID_H_

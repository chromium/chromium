// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_DESKTOP_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/enterprise/browser/reporting/profile_report_generator.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace base {
class FilePath;
}

namespace policy {
class MachineLevelUserCloudPolicyManager;
}

class Profile;

namespace enterprise_reporting {

/**
 * Desktop implementation of the profile reporting delegate.
 */
class ProfileReportGeneratorDesktop : public ProfileReportGenerator::Delegate {
 public:
  ProfileReportGeneratorDesktop();
  ProfileReportGeneratorDesktop(const ProfileReportGeneratorDesktop&) = delete;
  ProfileReportGeneratorDesktop& operator=(
      const ProfileReportGeneratorDesktop&) = delete;
  ~ProfileReportGeneratorDesktop() override;

  // ProfileReportGenerator::Delegate implementation.
  bool Init(const base::FilePath& path) override;
  void GetSigninUserInfo(
      enterprise_management::ChromeUserProfileInfo* report) override;
  void GetExtensionInfo(
      enterprise_management::ChromeUserProfileInfo* report) override;
  void GetExtensionRequest(
      enterprise_management::ChromeUserProfileInfo* report) override;
  std::unique_ptr<policy::PolicyConversionsClient> MakePolicyConversionsClient()
      override;
  policy::MachineLevelUserCloudPolicyManager* GetCloudPolicyManager() override;

 private:
  Profile* profile_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_DESKTOP_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_DELEGATE_BASE_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_DELEGATE_BASE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/reporting/profile_report_generator.h"

namespace base {
class FilePath;
}

namespace enterprise_management {
class ChromeUserProfileInfo;
}

namespace policy {
class PolicyConversionsClient;
}

namespace enterprise_reporting {

/**
 * Desktop & Android base implementation of the profile reporting delegate.
 */
class ProfileReportGeneratorDelegateBase
    : public ProfileReportGenerator::Delegate {
 public:
  ProfileReportGeneratorDelegateBase();
  ProfileReportGeneratorDelegateBase(
      const ProfileReportGeneratorDelegateBase&) = delete;
  ProfileReportGeneratorDelegateBase& operator=(
      const ProfileReportGeneratorDelegateBase&) = delete;
  ~ProfileReportGeneratorDelegateBase() override;

  // ProfileReportGenerator::Delegate implementation.
  bool Init(const base::FilePath& path) final;
  void GetSigninUserInfo(
      enterprise_management::ChromeUserProfileInfo* report) final;
  void GetAffiliationInfo(
      enterprise_management::ChromeUserProfileInfo* report) final;
  std::unique_ptr<policy::PolicyConversionsClient> MakePolicyConversionsClient(
      bool is_machine_scope) final;
  policy::CloudPolicyManager* GetCloudPolicyManager(
      bool is_machine_scope) final;

 protected:
  raw_ptr<Profile, DanglingUntriaged> profile_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_DELEGATE_BASE_H_

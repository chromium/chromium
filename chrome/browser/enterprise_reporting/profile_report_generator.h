// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace base {
class FilePath;
}

class Profile;

namespace enterprise_reporting {

/**
 * A report generator that collects Profile related information that is selected
 * by policies.
 */
class ProfileReportGenerator {
 public:

  ProfileReportGenerator();
  ~ProfileReportGenerator();

  void set_extensions_enabled(bool enabled);
  void set_policies_enabled(bool enabled);
  void set_extension_request_enabled(bool enabled);

  // Generates report for Profile if it's activated. Returns the report with
  // |callback| once it's ready. The report is null if it can't be generated.
  std::unique_ptr<em::ChromeUserProfileInfo> MaybeGenerate(
      const base::FilePath& path,
      const std::string& name);

 protected:
  // Get Signin information includes email and gaia id.
  virtual void GetSigninUserInfo();

  void GetExtensionInfo();
  void GetChromePolicyInfo();
  void GetExtensionPolicyInfo();
  void GetPolicyFetchTimestampInfo();
  void GetExtensionRequest();

 private:
  Profile* profile_;
  base::Value policies_;

  bool extensions_enabled_ = true;
  bool policies_enabled_ = true;
  bool extension_request_enabled_ = false;

  std::unique_ptr<em::ChromeUserProfileInfo> report_ = nullptr;

  base::WeakPtrFactory<ProfileReportGenerator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfileReportGenerator);
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_H_

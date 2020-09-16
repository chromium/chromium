// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/minimum_version_policy_test_helpers.h"

#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"

namespace policy {

base::Value CreateMinimumVersionPolicyRequirement(const std::string& version,
                                                  int warning,
                                                  int eol_warning) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(MinimumVersionPolicyHandler::kChromeOsVersion, version);
  dict.SetIntKey(MinimumVersionPolicyHandler::kWarningPeriod, warning);
  dict.SetIntKey(MinimumVersionPolicyHandler::kEolWarningPeriod, eol_warning);
  return dict;
}

base::Value CreateMinimumVersionPolicyValue(base::Value requirements,
                                            bool unmanaged_user_restricted) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(MinimumVersionPolicyHandler::kRequirements,
              std::move(requirements));
  dict.SetBoolKey(MinimumVersionPolicyHandler::kUnmanagedUserRestricted,
                  unmanaged_user_restricted);
  return dict;
}

base::Value CreateMinimumVersionSingleRequirementPolicyValue(
    const std::string& version,
    int warning,
    int eol_warning,
    bool unmanaged_user_restricted) {
  base::Value requirement_list(base::Value::Type::LIST);
  requirement_list.Append(
      CreateMinimumVersionPolicyRequirement(version, warning, eol_warning));
  return CreateMinimumVersionPolicyValue(std::move(requirement_list),
                                         unmanaged_user_restricted);
}

}  // namespace policy

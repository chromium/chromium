// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/minimum_version_policy_test_helpers.h"

#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"

namespace policy {

base::DictValue CreateMinimumVersionPolicyRequirement(
    const std::string& version,
    int warning,
    int eol_warning) {
  return base::DictValue()
      .Set(MinimumVersionPolicyHandler::kChromeOsVersion, version)
      .Set(MinimumVersionPolicyHandler::kWarningPeriod, warning)
      .Set(MinimumVersionPolicyHandler::kEolWarningPeriod, eol_warning);
}

base::DictValue CreateMinimumVersionPolicyValue(
    base::ListValue requirements,
    bool unmanaged_user_restricted) {
  return base::DictValue()
      .Set(MinimumVersionPolicyHandler::kRequirements, std::move(requirements))
      .Set(MinimumVersionPolicyHandler::kUnmanagedUserRestricted,
           unmanaged_user_restricted);
}

base::DictValue CreateMinimumVersionSingleRequirementPolicyValue(
    const std::string& version,
    int warning,
    int eol_warning,
    bool unmanaged_user_restricted) {
  auto requirement_list = base::ListValue().Append(
      CreateMinimumVersionPolicyRequirement(version, warning, eol_warning));
  return CreateMinimumVersionPolicyValue(std::move(requirement_list),
                                         unmanaged_user_restricted);
}

}  // namespace policy

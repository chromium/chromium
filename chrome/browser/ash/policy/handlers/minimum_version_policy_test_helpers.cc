// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/minimum_version_policy_test_helpers.h"

#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"

namespace policy {

base::Value::Dict CreateMinimumVersionPolicyRequirement(
    const std::string& version,
    int warning,
    int eol_warning) {
  return base::Value::Dict()
      .Set(MinimumVersionPolicyHandler::kChromeOsVersion, version)
      .Set(MinimumVersionPolicyHandler::kWarningPeriod, warning)
      .Set(MinimumVersionPolicyHandler::kEolWarningPeriod, eol_warning);
}

base::Value::Dict CreateMinimumVersionPolicyValue(
    base::Value::List requirements,
    bool unmanaged_user_restricted) {
  return base::Value::Dict()
      .Set(MinimumVersionPolicyHandler::kRequirements, std::move(requirements))
      .Set(MinimumVersionPolicyHandler::kUnmanagedUserRestricted,
           unmanaged_user_restricted);
}

base::Value::Dict CreateMinimumVersionSingleRequirementPolicyValue(
    const std::string& version,
    int warning,
    int eol_warning,
    bool unmanaged_user_restricted) {
  auto requirement_list = base::Value::List().Append(
      CreateMinimumVersionPolicyRequirement(version, warning, eol_warning));
  return CreateMinimumVersionPolicyValue(std::move(requirement_list),
                                         unmanaged_user_restricted);
}

}  // namespace policy

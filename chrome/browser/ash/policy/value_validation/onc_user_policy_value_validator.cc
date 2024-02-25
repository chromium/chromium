// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/value_validation/onc_user_policy_value_validator.h"

#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"

namespace em = enterprise_management;

namespace policy {

ONCUserPolicyValueValidator::ONCUserPolicyValueValidator()
    : ONCPolicyValueValidatorBase<em::CloudPolicySettings>(
          key::kOpenNetworkConfiguration,
          ::onc::ONCSource::ONC_SOURCE_USER_POLICY) {}

std::optional<std::string> ONCUserPolicyValueValidator::GetONCStringFromPayload(
    const em::CloudPolicySettings& policy_payload) const {
  if (policy_payload.has_opennetworkconfiguration()) {
    const em::StringPolicyProto& policy_proto =
        policy_payload.opennetworkconfiguration();
    if (policy_proto.has_value())
      return policy_proto.value();
  }
  return std::nullopt;
}

}  // namespace policy

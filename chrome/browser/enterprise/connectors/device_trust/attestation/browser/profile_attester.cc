// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/profile_attester.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace enterprise_connectors {

ProfileAttester::ProfileAttester(
    enterprise::ProfileIdService* profile_id_service,
    policy::CloudPolicyStore* user_cloud_policy_store)
    : profile_id_service_(profile_id_service),
      user_cloud_policy_store_(user_cloud_policy_store) {
  CHECK(profile_id_service_);
}

ProfileAttester::~ProfileAttester() = default;

void ProfileAttester::DecorateKeyInfo(const std::set<DTCPolicyLevel>& levels,
                                      KeyInfo& key_info,
                                      base::OnceClosure done_closure) {
  // TODO(b:279063303): Populate this with profile specific key info once the DT
  // proto changes occur.
  std::move(done_closure).Run();
}

void ProfileAttester::SignResponse(const std::set<DTCPolicyLevel>& levels,
                                   const std::string& challenge_response,
                                   SignedData& signed_data,
                                   base::OnceClosure done_closure) {
  // Profile level signatures are currently not supported.
  std::move(done_closure).Run();
}

}  // namespace enterprise_connectors

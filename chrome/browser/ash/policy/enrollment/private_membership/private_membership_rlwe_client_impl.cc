// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/private_membership/private_membership_rlwe_client_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/private_membership_rlwe_client.h"
#include "third_party/private_membership/src/membership_response_map.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"
#include "third_party/shell-encryption/src/statusor.h"

namespace psm_rlwe = private_membership::rlwe;

namespace policy {

PrivateMembershipRlweClientImpl::FactoryImpl::FactoryImpl() = default;

::rlwe::StatusOr<std::unique_ptr<PrivateMembershipRlweClient>>
PrivateMembershipRlweClientImpl::FactoryImpl::Create(
    psm_rlwe::RlweUseCase use_case,
    const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) {
  auto status_or_client =
      psm_rlwe::PrivateMembershipRlweClient::Create(use_case, plaintext_ids);
  if (!status_or_client.ok()) {
    return absl::InvalidArgumentError(status_or_client.status().message());
  }

  return absl::WrapUnique<PrivateMembershipRlweClient>(
      new PrivateMembershipRlweClientImpl(std::move(status_or_client).value()));
}

PrivateMembershipRlweClientImpl::FactoryImpl::~FactoryImpl() = default;

PrivateMembershipRlweClientImpl::~PrivateMembershipRlweClientImpl() = default;

::rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweOprfRequest>
PrivateMembershipRlweClientImpl::CreateOprfRequest() {
  return psm_rlwe_client_->CreateOprfRequest();
}

::rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweQueryRequest>
PrivateMembershipRlweClientImpl::CreateQueryRequest(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response) {
  return psm_rlwe_client_->CreateQueryRequest(oprf_response);
}

::rlwe::StatusOr<psm_rlwe::RlweMembershipResponses>
PrivateMembershipRlweClientImpl::ProcessQueryResponse(
    const psm_rlwe::PrivateMembershipRlweQueryResponse& query_response) {
  return psm_rlwe_client_->ProcessQueryResponse(query_response);
}

PrivateMembershipRlweClientImpl::PrivateMembershipRlweClientImpl(
    std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient> psm_rlwe_client)
    : psm_rlwe_client_(std::move(psm_rlwe_client)) {
  DCHECK(psm_rlwe_client_);
}

}  // namespace policy

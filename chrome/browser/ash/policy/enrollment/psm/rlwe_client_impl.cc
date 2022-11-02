// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/psm/rlwe_client_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_client.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"
#include "third_party/shell-encryption/src/statusor.h"

namespace psm_rlwe = private_membership::rlwe;

namespace policy::psm {

std::unique_ptr<RlweClient> RlweClientImpl::Create(
    const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) {
  auto status_or_client = psm_rlwe::PrivateMembershipRlweClient::Create(
      private_membership::rlwe::RlweUseCase::CROS_DEVICE_STATE, plaintext_ids);
  DCHECK(status_or_client.ok()) << status_or_client.status().message();

  return std::make_unique<psm::RlweClientImpl>(
      std::move(status_or_client).value());
}

std::unique_ptr<RlweClient> RlweClientImpl::CreateForTesting(
    const std::string& ec_cipher_key,
    const std::string& seed,
    const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) {
  auto status_or_client =
      psm_rlwe::PrivateMembershipRlweClient::CreateForTesting(
          private_membership::rlwe::RlweUseCase::CROS_DEVICE_STATE,
          plaintext_ids, ec_cipher_key, seed);
  DCHECK(status_or_client.ok()) << status_or_client.status().message();

  return std::make_unique<RlweClientImpl>(std::move(status_or_client).value());
}

RlweClientImpl::~RlweClientImpl() = default;

::rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweOprfRequest>
RlweClientImpl::CreateOprfRequest() {
  return psm_rlwe_client_->CreateOprfRequest();
}

::rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweQueryRequest>
RlweClientImpl::CreateQueryRequest(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response) {
  return psm_rlwe_client_->CreateQueryRequest(oprf_response);
}

::rlwe::StatusOr<psm_rlwe::RlweMembershipResponses>
RlweClientImpl::ProcessQueryResponse(
    const psm_rlwe::PrivateMembershipRlweQueryResponse& query_response) {
  return psm_rlwe_client_->ProcessQueryResponse(query_response);
}

RlweClientImpl::RlweClientImpl(
    std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient> psm_rlwe_client)
    : psm_rlwe_client_(std::move(psm_rlwe_client)) {
  DCHECK(psm_rlwe_client_);
}

}  // namespace policy::psm

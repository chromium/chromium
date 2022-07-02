// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/private_membership/testing_private_membership_rlwe_client.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/private_membership_rlwe_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"
#include "third_party/shell-encryption/src/statusor.h"

namespace psm_rlwe = private_membership::rlwe;

namespace policy {

TestingPrivateMembershipRlweClient::FactoryImpl::FactoryImpl(
    const std::string& ec_cipher_key,
    const std::string& seed,
    std::vector<private_membership::rlwe::RlwePlaintextId>
        plaintext_testing_ids)
    : ec_cipher_key_(ec_cipher_key),
      seed_(seed),
      plaintext_testing_ids_(std::move(plaintext_testing_ids)) {}

::rlwe::StatusOr<std::unique_ptr<PrivateMembershipRlweClient>>
TestingPrivateMembershipRlweClient::FactoryImpl::Create(
    psm_rlwe::RlweUseCase use_case,
    const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) {
  auto status_or_client =
      psm_rlwe::PrivateMembershipRlweClient::CreateForTesting(
          use_case, plaintext_testing_ids_, ec_cipher_key_, seed_);
  if (!status_or_client.ok()) {
    return absl::InvalidArgumentError(status_or_client.status().message());
  }
  return absl::WrapUnique<PrivateMembershipRlweClient>(
      new TestingPrivateMembershipRlweClient(
          std::move(status_or_client).value()));
}

TestingPrivateMembershipRlweClient::FactoryImpl::~FactoryImpl() = default;

TestingPrivateMembershipRlweClient::~TestingPrivateMembershipRlweClient() =
    default;

::rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweOprfRequest>
TestingPrivateMembershipRlweClient::CreateOprfRequest() {
  return psm_rlwe_client_->CreateOprfRequest();
}

::rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweQueryRequest>
TestingPrivateMembershipRlweClient::CreateQueryRequest(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response) {
  return psm_rlwe_client_->CreateQueryRequest(oprf_response);
}

::rlwe::StatusOr<psm_rlwe::RlweMembershipResponses>
TestingPrivateMembershipRlweClient::ProcessQueryResponse(
    const psm_rlwe::PrivateMembershipRlweQueryResponse& query_response) {
  return psm_rlwe_client_->ProcessQueryResponse(query_response);
}

TestingPrivateMembershipRlweClient::TestingPrivateMembershipRlweClient(
    std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient> psm_rlwe_client)
    : psm_rlwe_client_(std::move(psm_rlwe_client)) {
  DCHECK(psm_rlwe_client_);
}

}  // namespace policy

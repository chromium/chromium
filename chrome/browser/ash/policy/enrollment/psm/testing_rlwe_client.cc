// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/psm/testing_rlwe_client.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"
#include "third_party/shell-encryption/src/statusor.h"

namespace psm_rlwe = private_membership::rlwe;

namespace policy::psm {

TestingRlweClient::FactoryImpl::FactoryImpl(
    const std::string& ec_cipher_key,
    const std::string& seed,
    std::vector<PlaintextId> plaintext_testing_ids)
    : ec_cipher_key_(ec_cipher_key),
      seed_(seed),
      plaintext_testing_ids_(std::move(plaintext_testing_ids)) {}

::rlwe::StatusOr<std::unique_ptr<RlweClient>>
TestingRlweClient::FactoryImpl::Create(
    psm_rlwe::RlweUseCase use_case,
    const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) {
  auto status_or_client =
      psm_rlwe::PrivateMembershipRlweClient::CreateForTesting(
          use_case, plaintext_testing_ids_, ec_cipher_key_, seed_);
  if (!status_or_client.ok()) {
    return absl::InvalidArgumentError(status_or_client.status().message());
  }
  return absl::WrapUnique<RlweClient>(
      new TestingRlweClient(std::move(status_or_client).value()));
}

TestingRlweClient::FactoryImpl::~FactoryImpl() = default;

TestingRlweClient::~TestingRlweClient() = default;

::rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweOprfRequest>
TestingRlweClient::CreateOprfRequest() {
  return psm_rlwe_client_->CreateOprfRequest();
}

::rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweQueryRequest>
TestingRlweClient::CreateQueryRequest(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response) {
  return psm_rlwe_client_->CreateQueryRequest(oprf_response);
}

::rlwe::StatusOr<psm_rlwe::RlweMembershipResponses>
TestingRlweClient::ProcessQueryResponse(
    const psm_rlwe::PrivateMembershipRlweQueryResponse& query_response) {
  return psm_rlwe_client_->ProcessQueryResponse(query_response);
}

TestingRlweClient::TestingRlweClient(
    std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient> psm_rlwe_client)
    : psm_rlwe_client_(std::move(psm_rlwe_client)) {
  DCHECK(psm_rlwe_client_);
}

}  // namespace policy::psm

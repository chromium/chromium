// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/private_membership/fake_private_membership_rlwe_client.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/private_membership_rlwe_client.h"
#include "third_party/private_membership/src/membership_response_map.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/shell-encryption/src/statusor.h"

namespace psm_rlwe = private_membership::rlwe;

namespace {
// Constants that represent the expected signal data from the server as a
// membership or not. The signal data is inside PirResponse.plaintext_entry_size
// field.
const int kHasMembership = 1;
const int kHasNoMembership = 2;
}  // namespace

namespace policy {

::rlwe::StatusOr<std::unique_ptr<PrivateMembershipRlweClient>>
FakePrivateMembershipRlweClient::FactoryImpl::Create(
    psm_rlwe::RlweUseCase use_case,
    const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) {
  return absl::WrapUnique<PrivateMembershipRlweClient>(
      new FakePrivateMembershipRlweClient(use_case, plaintext_ids));
}

FakePrivateMembershipRlweClient::~FakePrivateMembershipRlweClient() = default;

::rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweOprfRequest>
FakePrivateMembershipRlweClient::CreateOprfRequest() {
  psm_rlwe::PrivateMembershipRlweOprfRequest request;
  request.set_use_case(use_case_);

  // Send the plaintext ID as the only encrypted ID.
  const std::string encrypted_id = plaintext_ids_[0].sensitive_id();
  *request.add_encrypted_ids() = encrypted_id;

  return request;
}

::rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweQueryRequest>
FakePrivateMembershipRlweClient::CreateQueryRequest(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response) {
  psm_rlwe::PrivateMembershipRlweQueryRequest request;
  request.set_use_case(use_case_);

  const std::string encrypted_id =
      oprf_response.doubly_encrypted_ids(0).queried_encrypted_id();

  // Check validity of returned queried ID.
  if (encrypted_id != plaintext_ids_[0].sensitive_id()) {
    return absl::InvalidArgumentError(
        "OPRF response contains a response to an erroneous encrypted ID.");
  }

  // Send the same encrypted_id again to indicate the validality of the received
  // |oprf_response|.
  psm_rlwe::PrivateMembershipRlweQuery single_query;
  single_query.set_queried_encrypted_id(encrypted_id);
  *request.add_queries() = single_query;

  return request;
}

::rlwe::StatusOr<psm_rlwe::RlweMembershipResponses>
FakePrivateMembershipRlweClient::ProcessQueryResponse(
    const psm_rlwe::PrivateMembershipRlweQueryResponse& query_response) {
  // Validate that we have an existing response.
  if (!query_response.pir_responses_size()) {
    return absl::InvalidArgumentError(
        "Query response missing a response to a requested ID.");
  }

  const auto& pir_response = query_response.pir_responses(0);
  const std::string encrypted_id = pir_response.queried_encrypted_id();

  // Check validity of returned queried ID.
  if (encrypted_id != plaintext_ids_[0].sensitive_id()) {
    return absl::InvalidArgumentError(
        "Query response contains a response to an erroneous encrypted ID.");
  }

  // Server should fill out PirResponse message with plaintext_entry_size field
  // to indicate the membership response.
  const int server_membership_response =
      pir_response.pir_response().plaintext_entry_size();

  // Check validity of the server membership response.
  if (server_membership_response != kHasMembership &&
      server_membership_response != kHasNoMembership) {
    return absl::InvalidArgumentError(
        "Query response contains unknown membership response to the queried "
        "encrypted ID.");
  }

  // Include one membership response in RlweMembershipResponses. Its
  // plaintext_id field will be the first entry of |plaintext_ids_|.

  psm_rlwe::RlweMembershipResponses rlwe_membership_responses;
  auto* entry = rlwe_membership_responses.add_membership_responses();

  entry->mutable_plaintext_id()->set_sensitive_id(
      plaintext_ids_[0].sensitive_id());
  entry->mutable_membership_response()->set_is_member(
      server_membership_response == kHasMembership);
  entry->mutable_membership_response()->set_value(encrypted_id);

  return rlwe_membership_responses;
}

FakePrivateMembershipRlweClient::FakePrivateMembershipRlweClient(
    psm_rlwe::RlweUseCase use_case,
    std::vector<psm_rlwe::RlwePlaintextId> plaintext_ids)
    : use_case_(use_case), plaintext_ids_(std::move(plaintext_ids)) {
  // Verify that we have at least one instance of RlwePlaintextId.
  DCHECK(!plaintext_ids_.empty());
}

}  // namespace policy

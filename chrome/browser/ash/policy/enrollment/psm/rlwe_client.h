// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_CLIENT_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_CLIENT_H_

#include <memory>
#include <vector>

#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/shell-encryption/src/statusor.h"

namespace private_membership::rlwe {
class RlweMembershipResponses;
}  // namespace private_membership::rlwe

namespace policy::psm {

// Interface for the Private Membership RLWE Client, allowing to replace the
// private membership RLWE client library with a fake for tests.
class RlweClient {
 public:
  using UseCase = private_membership::rlwe::RlweUseCase;
  using PlaintextId = private_membership::rlwe::RlwePlaintextId;
  using OprfRequest =
      private_membership::rlwe::PrivateMembershipRlweOprfRequest;
  using OprfResponse =
      private_membership::rlwe::PrivateMembershipRlweOprfResponse;
  using QueryRequest =
      private_membership::rlwe::PrivateMembershipRlweQueryRequest;
  using QueryResponse =
      private_membership::rlwe::PrivateMembershipRlweQueryResponse;
  using MembershipResponses = private_membership::rlwe::RlweMembershipResponses;

  class Factory {
   public:
    virtual ~Factory() = default;
    // Creates a client for the Private Membership RLWE protocol. It will be
    // created for |plaintext_ids| with use case as |use_case|.
    virtual ::rlwe::StatusOr<std::unique_ptr<RlweClient>> Create(
        UseCase use_case,
        const std::vector<PlaintextId>& plaintext_ids) = 0;
  };

  virtual ~RlweClient() = default;

  // Creates a request proto for the first phase of the protocol.
  virtual ::rlwe::StatusOr<OprfRequest> CreateOprfRequest() = 0;

  // Creates a request proto for the second phase of the protocol.
  virtual ::rlwe::StatusOr<QueryRequest> CreateQueryRequest(
      const OprfResponse& oprf_response) = 0;

  // Processes the query response from the server and returns the membership
  // response map.
  //
  // Keys of the returned map match the original plaintext ids supplied to the
  // client when it was created.
  virtual ::rlwe::StatusOr<MembershipResponses> ProcessQueryResponse(
      const QueryResponse& query_response) = 0;
};

}  // namespace policy::psm

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_CLIENT_H_

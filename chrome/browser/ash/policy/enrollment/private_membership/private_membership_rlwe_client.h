// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PRIVATE_MEMBERSHIP_RLWE_CLIENT_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PRIVATE_MEMBERSHIP_RLWE_CLIENT_H_

#include <memory>
#include <vector>

#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/shell-encryption/src/statusor.h"

namespace private_membership {
namespace rlwe {
class RlweMembershipResponses;
}  // namespace rlwe
}  // namespace private_membership

namespace policy {

// Interface for the Private Membership RLWE Client, allowing to replace the
// private membership RLWE client library with a fake for tests.
class PrivateMembershipRlweClient {
 public:
  class Factory {
   public:
    virtual ~Factory() = default;
    // Creates a client for the Private Membership RLWE protocol. It will be
    // created for |plaintext_ids| with use case as |use_case|.
    virtual ::rlwe::StatusOr<std::unique_ptr<PrivateMembershipRlweClient>>
    Create(private_membership::rlwe::RlweUseCase use_case,
           const std::vector<private_membership::rlwe::RlwePlaintextId>&
               plaintext_ids) = 0;
  };

  virtual ~PrivateMembershipRlweClient() = default;

  // Creates a request proto for the first phase of the protocol.
  virtual ::rlwe::StatusOr<
      private_membership::rlwe::PrivateMembershipRlweOprfRequest>
  CreateOprfRequest() = 0;

  // Creates a request proto for the second phase of the protocol.
  virtual ::rlwe::StatusOr<
      private_membership::rlwe::PrivateMembershipRlweQueryRequest>
  CreateQueryRequest(
      const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
          oprf_response) = 0;

  // Processes the query response from the server and returns the membership
  // response map.
  //
  // Keys of the returned map match the original plaintext ids supplied to the
  // client when it was created.
  virtual ::rlwe::StatusOr<private_membership::rlwe::RlweMembershipResponses>
  ProcessQueryResponse(
      const private_membership::rlwe::PrivateMembershipRlweQueryResponse&
          query_response) = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PRIVATE_MEMBERSHIP_RLWE_CLIENT_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_FAKE_PRIVATE_MEMBERSHIP_RLWE_CLIENT_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_FAKE_PRIVATE_MEMBERSHIP_RLWE_CLIENT_H_

#include "chrome/browser/ash/policy/enrollment/private_membership/private_membership_rlwe_client.h"

#include <string>
#include <vector>

#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/shell-encryption/src/statusor.h"

namespace private_membership {
namespace rlwe {
class RlweMembershipResponses;
}  // namespace rlwe
}  // namespace private_membership

namespace policy {

class FakePrivateMembershipRlweClient : public PrivateMembershipRlweClient {
 public:
  // A factory that creates |FakePrivateMembershipRlweClient|s.
  class FactoryImpl : public Factory {
   public:
    FactoryImpl() = default;

    // FactoryImpl is neither copyable nor copy assignable.
    FactoryImpl(const FactoryImpl&) = delete;
    FactoryImpl& operator=(const FactoryImpl&) = delete;

    ~FactoryImpl() override = default;

    // Creates a fake PSM RLWE client for testing purposes.
    ::rlwe::StatusOr<std::unique_ptr<PrivateMembershipRlweClient>> Create(
        private_membership::rlwe::RlweUseCase use_case,
        const std::vector<private_membership::rlwe::RlwePlaintextId>&
            plaintext_ids) override;
  };

  // FakePrivateMembershipRlweClient is neither copyable nor copy assignable.
  FakePrivateMembershipRlweClient(const FakePrivateMembershipRlweClient&) =
      delete;
  FakePrivateMembershipRlweClient& operator=(
      const FakePrivateMembershipRlweClient&) = delete;

  ~FakePrivateMembershipRlweClient() override;

  // Mocks all function calls of PrivateMembershipRlweClient.

  ::rlwe::StatusOr<private_membership::rlwe::PrivateMembershipRlweOprfRequest>
  CreateOprfRequest() override;
  ::rlwe::StatusOr<private_membership::rlwe::PrivateMembershipRlweQueryRequest>
  CreateQueryRequest(
      const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
          oprf_response) override;
  ::rlwe::StatusOr<private_membership::rlwe::RlweMembershipResponses>
  ProcessQueryResponse(
      const private_membership::rlwe::PrivateMembershipRlweQueryResponse&
          query_response) override;

 private:
  FakePrivateMembershipRlweClient(
      private_membership::rlwe::RlweUseCase use_case,
      std::vector<private_membership::rlwe::RlwePlaintextId> plaintext_ids);

  const private_membership::rlwe::RlweUseCase use_case_;
  const std::vector<private_membership::rlwe::RlwePlaintextId> plaintext_ids_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_FAKE_PRIVATE_MEMBERSHIP_RLWE_CLIENT_H_

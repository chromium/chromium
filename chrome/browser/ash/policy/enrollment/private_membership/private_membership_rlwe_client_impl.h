// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PRIVATE_MEMBERSHIP_RLWE_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PRIVATE_MEMBERSHIP_RLWE_CLIENT_IMPL_H_

#include "chrome/browser/ash/policy/enrollment/private_membership/private_membership_rlwe_client.h"

#include <memory>
#include <string>
#include <vector>

#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/shell-encryption/src/statusor.h"

namespace private_membership {
namespace rlwe {
class PrivateMembershipRlweClient;
class RlweMembershipResponses;
}  // namespace rlwe
}  // namespace private_membership

namespace policy {

class PrivateMembershipRlweClientImpl : public PrivateMembershipRlweClient {
 public:
  // A factory that creates |PrivateMembershipRlweClientImpl|s.
  class FactoryImpl : public Factory {
   public:
    FactoryImpl();

    // FactoryImpl is neither copyable nor copy assignable.
    FactoryImpl(const FactoryImpl&) = delete;
    FactoryImpl& operator=(const FactoryImpl&) = delete;

    ~FactoryImpl() override;

    // Creates PSM RLWE client that generates and holds a randomly generated
    // key.
    ::rlwe::StatusOr<std::unique_ptr<PrivateMembershipRlweClient>> Create(
        private_membership::rlwe::RlweUseCase use_case,
        const std::vector<private_membership::rlwe::RlwePlaintextId>&
            plaintext_ids) override;
  };

  // PrivateMembershipRlweClientImpl is neither copyable nor copy assignable.
  PrivateMembershipRlweClientImpl(const PrivateMembershipRlweClientImpl&) =
      delete;
  PrivateMembershipRlweClientImpl& operator=(
      const PrivateMembershipRlweClientImpl&) = delete;

  ~PrivateMembershipRlweClientImpl() override;

  // Delegates all function calls into PrivateMembershipRlweClient by
  // |psm_rlwe_client_|.

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
  explicit PrivateMembershipRlweClientImpl(
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
          psm_rlwe_client);

  const std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
      psm_rlwe_client_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PRIVATE_MEMBERSHIP_RLWE_CLIENT_IMPL_H_

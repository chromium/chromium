// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_TESTING_RLWE_CLIENT_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_TESTING_RLWE_CLIENT_H_

#include "chrome/browser/ash/policy/enrollment/psm/rlwe_client.h"

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

namespace policy::psm {

class TestingRlweClient : public RlweClient {
 public:
  // A factory that creates |TestingRlweClient|s.
  class FactoryImpl : public Factory {
   public:
    // TODO(crbug.com/1239329): Remove |plaintext_ids| from the factory
    // constructor, and create a delegate for PSM ID.
    FactoryImpl(const std::string& ec_cipher_key,
                const std::string& seed,
                std::vector<private_membership::rlwe::RlwePlaintextId>
                    plaintext_testing_ids);

    // FactoryImpl is neither copyable nor copy assignable.
    FactoryImpl(const FactoryImpl&) = delete;
    FactoryImpl& operator=(const FactoryImpl&) = delete;

    ~FactoryImpl() override;

    // Creates PSM RLWE client for testing with a given cipher key
    // |ec_cipher_key_| and deterministic PRNG |seed_|.
    // Note: |plaintext_ids| value will be ignored while creating the client,
    // and |plaintext_testing_ids_| member will be used instead for testing.
    ::rlwe::StatusOr<std::unique_ptr<RlweClient>> Create(
        private_membership::rlwe::RlweUseCase use_case,
        const std::vector<private_membership::rlwe::RlwePlaintextId>&
            plaintext_ids) override;

   private:
    // The following members are used to create the PSM RLWE client for testing.

    const std::string ec_cipher_key_;
    const std::string seed_;
    const std::vector<private_membership::rlwe::RlwePlaintextId>
        plaintext_testing_ids_;
  };

  // TestingRlweClient is neither copyable nor copy assignable.
  TestingRlweClient(const TestingRlweClient&) = delete;
  TestingRlweClient& operator=(const TestingRlweClient&) = delete;

  ~TestingRlweClient() override;

  // Delegates all function calls into RlweClient by
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
  explicit TestingRlweClient(
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
          psm_rlwe_client);

  const std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
      psm_rlwe_client_;
};

}  // namespace policy::psm

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_TESTING_RLWE_CLIENT_H_

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

namespace private_membership::rlwe {
class PrivateMembershipRlweClient;
}  // namespace private_membership::rlwe

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
                std::vector<PlaintextId> plaintext_testing_ids);

    // FactoryImpl is neither copyable nor copy assignable.
    FactoryImpl(const FactoryImpl&) = delete;
    FactoryImpl& operator=(const FactoryImpl&) = delete;

    ~FactoryImpl() override;

    // Creates PSM RLWE client for testing with a given cipher key
    // |ec_cipher_key_| and deterministic PRNG |seed_|.
    // Note: |plaintext_ids| value will be ignored while creating the client,
    // and |plaintext_testing_ids_| member will be used instead for testing.
    ::rlwe::StatusOr<std::unique_ptr<RlweClient>> Create(
        UseCase use_case,
        const std::vector<PlaintextId>& plaintext_ids) override;

   private:
    // The following members are used to create the PSM RLWE client for testing.

    const std::string ec_cipher_key_;
    const std::string seed_;
    const std::vector<PlaintextId> plaintext_testing_ids_;
  };

  // TestingRlweClient is neither copyable nor copy assignable.
  TestingRlweClient(const TestingRlweClient&) = delete;
  TestingRlweClient& operator=(const TestingRlweClient&) = delete;

  ~TestingRlweClient() override;

  // Delegates all function calls into RlweClient by
  // |psm_rlwe_client_|.

  ::rlwe::StatusOr<OprfRequest> CreateOprfRequest() override;
  ::rlwe::StatusOr<QueryRequest> CreateQueryRequest(
      const OprfResponse& oprf_response) override;
  ::rlwe::StatusOr<MembershipResponses> ProcessQueryResponse(
      const QueryResponse& query_response) override;

 private:
  explicit TestingRlweClient(
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
          psm_rlwe_client);

  const std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
      psm_rlwe_client_;
};

}  // namespace policy::psm

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_TESTING_RLWE_CLIENT_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_CLIENT_IMPL_H_

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

class RlweClientImpl : public RlweClient {
 public:
  // Creates PSM RLWE client that generates and holds a randomly generated
  // key.
  static std::unique_ptr<RlweClient> Create(const PlaintextId& plaintext_id);

  // In contrast to `Create` this creates a PSM RLWE client for testing with a
  // fixed `ec_cipher_key` and `seed`.
  static std::unique_ptr<RlweClient> CreateForTesting(
      const std::string& ec_cipher_key,
      const std::string& seed,
      const PlaintextId& plaintext_id);

  explicit RlweClientImpl(
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
          psm_rlwe_client,
      const PlaintextId& plaintext_id);

  RlweClientImpl(const RlweClientImpl&) = delete;
  RlweClientImpl& operator=(const RlweClientImpl&) = delete;

  ~RlweClientImpl() override;

  // Delegates all function calls into RlweClient by
  // |psm_rlwe_client_|.

  ::rlwe::StatusOr<OprfRequest> CreateOprfRequest() override;
  ::rlwe::StatusOr<QueryRequest> CreateQueryRequest(
      const OprfResponse& oprf_response) override;
  ::rlwe::StatusOr<bool> ProcessQueryResponse(
      const QueryResponse& query_response) override;
  std::string GetPsmIdentifierDebugString() override;

 private:
  const std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
      psm_rlwe_client_;
  PlaintextId plaintext_id_;
};

}  // namespace policy::psm

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_CLIENT_IMPL_H_

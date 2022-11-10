// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_FAKE_RLWE_CLIENT_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_FAKE_RLWE_CLIENT_H_

#include "chrome/browser/ash/policy/enrollment/psm/rlwe_client.h"

#include <string>
#include <vector>

#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/shell-encryption/src/statusor.h"

namespace policy::psm {

class FakeRlweClient : public RlweClient {
 public:
  // Creates a fake PSM RLWE client for testing purposes.
  static std::unique_ptr<RlweClient> Create(const PlaintextId& plaintext_id);

  FakeRlweClient(UseCase use_case, const PlaintextId& plaintext_id);
  FakeRlweClient(const FakeRlweClient&) = delete;
  FakeRlweClient& operator=(const FakeRlweClient&) = delete;
  ~FakeRlweClient() override;

  // Mocks all function calls of RlweClient.
  ::rlwe::StatusOr<OprfRequest> CreateOprfRequest() override;
  ::rlwe::StatusOr<QueryRequest> CreateQueryRequest(
      const OprfResponse& oprf_response) override;
  ::rlwe::StatusOr<bool> ProcessQueryResponse(
      const QueryResponse& query_response) override;

 private:
  const UseCase use_case_;
  const PlaintextId plaintext_id_;
};

}  // namespace policy::psm

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_FAKE_RLWE_CLIENT_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_FAKE_RLWE_DMSERVER_CLIENT_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_FAKE_RLWE_DMSERVER_CLIENT_H_

#include "chrome/browser/ash/policy/enrollment/psm/rlwe_dmserver_client.h"

namespace policy::psm {

class FakeRlweDmserverClient : public RlweDmserverClient {
 public:
  FakeRlweDmserverClient();

  // Disallow copy constructor and assignment operator.
  FakeRlweDmserverClient(const FakeRlweDmserverClient&) = delete;
  FakeRlweDmserverClient& operator=(const FakeRlweDmserverClient&) = delete;

  ~FakeRlweDmserverClient() override = default;

  // Executes the `callback` immediately with `completion_params_`;
  void CheckMembership(CompletionCallback callback) override;

  // Always returns false.
  bool IsCheckMembershipInProgress() const override;

  // Overrides the `result_holder_` values.
  void WillReplyWith(ResultHolder new_result_holder);

 private:
  // Final PSM result that is used for tests.
  ResultHolder result_holder_;
};

}  // namespace policy::psm

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_FAKE_RLWE_DMSERVER_CLIENT_H_

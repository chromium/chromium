// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/private_membership/fake_psm_rlwe_dmserver_client.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_dmserver_client.h"

namespace policy {

FakePsmRlweDmserverClient::FakePsmRlweDmserverClient()
    : result_holder_(PsmResult::kConnectionError) {}

void FakePsmRlweDmserverClient::CheckMembership(CompletionCallback callback) {
  DCHECK(callback);
  std::move(callback).Run(result_holder_);
}

bool FakePsmRlweDmserverClient::IsCheckMembershipInProgress() const {
  return false;
}

void FakePsmRlweDmserverClient::WillReplyWith(ResultHolder new_result_holder) {
  result_holder_ = std::move(new_result_holder);
}

}  // namespace policy

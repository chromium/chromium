// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/psm/fake_rlwe_dmserver_client.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_dmserver_client.h"

namespace policy::psm {

FakeRlweDmserverClient::FakeRlweDmserverClient()
    : result_holder_(psm::RlweResult::kConnectionError) {}

void FakeRlweDmserverClient::CheckMembership(CompletionCallback callback) {
  DCHECK(callback);
  std::move(callback).Run(result_holder_);
}

bool FakeRlweDmserverClient::IsCheckMembershipInProgress() const {
  return false;
}

void FakeRlweDmserverClient::WillReplyWith(ResultHolder new_result_holder) {
  result_holder_ = std::move(new_result_holder);
}

}  // namespace policy::psm

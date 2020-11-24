// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/session/arc_provisioning_result.h"

#include "base/check.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"

namespace arc {

ArcProvisioningResult::ArcProvisioningResult(mojom::ArcSignInResultPtr result)
    : result_(std::move(result)) {}
ArcProvisioningResult::ArcProvisioningResult(ArcStopReason reason)
    : result_(reason) {}
ArcProvisioningResult::ArcProvisioningResult(OverallSignInTimeout timeout)
    : result_(timeout) {}
ArcProvisioningResult::ArcProvisioningResult(ArcProvisioningResult&& other) =
    default;
ArcProvisioningResult::~ArcProvisioningResult() = default;

bool ArcProvisioningResult::has_signin_result() const {
  return absl::holds_alternative<mojom::ArcSignInResultPtr>(result_);
}

const mojom::ArcSignInResult* ArcProvisioningResult::signin_result() const {
  DCHECK(has_signin_result());
  return absl::get<mojom::ArcSignInResultPtr>(result_).get();
}

bool ArcProvisioningResult::has_signin_error() const {
  return has_signin_result() && signin_result()->is_error();
}

const mojom::ArcSignInError* ArcProvisioningResult::signin_error() const {
  DCHECK(has_signin_error());
  return signin_result()->get_error().get();
}

bool ArcProvisioningResult::is_success() const {
  return has_signin_result() && signin_result()->is_success();
}

bool ArcProvisioningResult::has_general_error(
    mojom::GeneralSignInError error) const {
  return has_signin_error() && signin_error()->is_general_error() &&
         signin_error()->get_general_error() == error;
}

bool ArcProvisioningResult::is_stopped() const {
  return absl::holds_alternative<ArcStopReason>(result_);
}

ArcStopReason ArcProvisioningResult::stop_reason() const {
  DCHECK(is_stopped());
  return absl::get<ArcStopReason>(result_);
}

bool ArcProvisioningResult::is_timedout() const {
  return absl::holds_alternative<OverallSignInTimeout>(result_);
}

std::ostream& operator<<(std::ostream& os,
                         const ArcProvisioningResult& result) {
  return os << GetProvisioningResultUMA(result);
}

}  // namespace arc

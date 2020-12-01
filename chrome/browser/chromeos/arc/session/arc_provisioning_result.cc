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
ArcProvisioningResult::ArcProvisioningResult(ChromeProvisioningTimeout timeout)
    : result_(timeout) {}
ArcProvisioningResult::ArcProvisioningResult(ArcProvisioningResult&& other) =
    default;
ArcProvisioningResult::~ArcProvisioningResult() = default;

bool ArcProvisioningResult::has_sign_in_result() const {
  return absl::holds_alternative<mojom::ArcSignInResultPtr>(result_);
}

const mojom::ArcSignInResult* ArcProvisioningResult::sign_in_result() const {
  DCHECK(has_sign_in_result());
  return absl::get<mojom::ArcSignInResultPtr>(result_).get();
}

bool ArcProvisioningResult::has_sign_in_error() const {
  return has_sign_in_result() && sign_in_result()->is_error();
}

const mojom::ArcSignInError* ArcProvisioningResult::sign_in_error() const {
  DCHECK(has_sign_in_error());
  return sign_in_result()->get_error().get();
}

bool ArcProvisioningResult::is_success() const {
  return has_sign_in_result() && sign_in_result()->is_success();
}

bool ArcProvisioningResult::has_general_error(
    mojom::GeneralSignInError error) const {
  return has_sign_in_error() && sign_in_error()->is_general_error() &&
         sign_in_error()->get_general_error() == error;
}

bool ArcProvisioningResult::is_stopped() const {
  return absl::holds_alternative<ArcStopReason>(result_);
}

ArcStopReason ArcProvisioningResult::stop_reason() const {
  DCHECK(is_stopped());
  return absl::get<ArcStopReason>(result_);
}

bool ArcProvisioningResult::is_timedout() const {
  return absl::holds_alternative<ChromeProvisioningTimeout>(result_);
}

std::ostream& operator<<(std::ostream& os,
                         const ArcProvisioningResult& result) {
  return os << GetProvisioningResultUMA(result);
}

}  // namespace arc

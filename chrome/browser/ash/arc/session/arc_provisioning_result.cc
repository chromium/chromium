// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_provisioning_result.h"

#include "base/check.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"

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

std::optional<mojom::GMSSignInError> ArcProvisioningResult::gms_sign_in_error()
    const {
  if (!sign_in_error() || !sign_in_error()->is_sign_in_error())
    return std::nullopt;

  return sign_in_error()->get_sign_in_error();
}

std::optional<mojom::GMSCheckInError>
ArcProvisioningResult::gms_check_in_error() const {
  if (!sign_in_error() || !sign_in_error()->is_check_in_error())
    return std::nullopt;

  return sign_in_error()->get_check_in_error();
}

std::optional<mojom::CloudProvisionFlowError>
ArcProvisioningResult::cloud_provision_flow_error() const {
  if (!sign_in_error() || !sign_in_error()->is_cloud_provision_flow_error())
    return std::nullopt;

  return sign_in_error()->get_cloud_provision_flow_error();
}

const mojom::ArcSignInError* ArcProvisioningResult::sign_in_error() const {
  if (!sign_in_result() || !sign_in_result()->is_error())
    return nullptr;

  return sign_in_result()->get_error().get();
}

std::optional<mojom::GeneralSignInError> ArcProvisioningResult::general_error()
    const {
  if (!sign_in_error() || !sign_in_error()->is_general_error())
    return std::nullopt;

  return sign_in_error()->get_general_error();
}

bool ArcProvisioningResult::is_success() const {
  return sign_in_result() && sign_in_result()->is_success();
}

std::optional<ArcStopReason> ArcProvisioningResult::stop_reason() const {
  if (!absl::holds_alternative<ArcStopReason>(result_))
    return std::nullopt;

  return absl::get<ArcStopReason>(result_);
}

bool ArcProvisioningResult::is_timedout() const {
  return absl::holds_alternative<ChromeProvisioningTimeout>(result_);
}

const mojom::ArcSignInResult* ArcProvisioningResult::sign_in_result() const {
  if (!absl::holds_alternative<mojom::ArcSignInResultPtr>(result_))
    return nullptr;

  return absl::get<mojom::ArcSignInResultPtr>(result_).get();
}

std::ostream& operator<<(std::ostream& os,
                         const ArcProvisioningResult& result) {
  return os << GetProvisioningStatus(result);
}

}  // namespace arc

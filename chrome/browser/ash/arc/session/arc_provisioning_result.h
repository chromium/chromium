// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_PROVISIONING_RESULT_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_PROVISIONING_RESULT_H_

#include <ostream>

#include "base/optional.h"
#include "components/arc/mojom/auth.mojom.h"
#include "components/arc/session/arc_stop_reason.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

enum class ProvisioningStatus;

namespace arc {

// A struct that represents timeout of ARC provisioning from Chrome.
struct ChromeProvisioningTimeout {};

// A class that encapsulates the result of provisioning ARC.
class ArcProvisioningResult {
 public:
  explicit ArcProvisioningResult(mojom::ArcSignInResultPtr result);
  explicit ArcProvisioningResult(ArcStopReason reason);
  explicit ArcProvisioningResult(ChromeProvisioningTimeout timeout);
  ArcProvisioningResult(ArcProvisioningResult&& other);
  ~ArcProvisioningResult();

  // Returns gms sign-in error if sign-in result has it.
  base::Optional<mojom::GMSSignInError> gms_sign_in_error() const;

  // Returns gms check-in error if sign-in result has it.
  base::Optional<mojom::GMSCheckInError> gms_check_in_error() const;

  // Returns cloud provision flow error if sign-in result has it.
  base::Optional<mojom::CloudProvisionFlowError> cloud_provision_flow_error()
      const;

  // Returns the error of signin_result coming from ARC.
  const mojom::ArcSignInError* sign_in_error() const;

  // Returns general sign-in error if result has it.
  base::Optional<mojom::GeneralSignInError> general_error() const;

  // Returns true if provisioning was successful.
  bool is_success() const;

  // Returns the reason for ARC stopped event if it exists.
  base::Optional<ArcStopReason> stop_reason() const;

  // Returns true if ARC provisioning timed out in Chrome.
  bool is_timedout() const;

 private:
  // Returns the result of provisioning from inside ARC.
  const mojom::ArcSignInResult* sign_in_result() const;

  absl::variant<mojom::ArcSignInResultPtr,
                ArcStopReason,
                ChromeProvisioningTimeout>
      result_;
};

// Outputs the stringified |result| to |os|. This is only for logging purposes.
std::ostream& operator<<(std::ostream& os, const ArcProvisioningResult& result);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_PROVISIONING_RESULT_H_

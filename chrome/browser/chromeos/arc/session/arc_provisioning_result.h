// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_SESSION_ARC_PROVISIONING_RESULT_H_
#define CHROME_BROWSER_CHROMEOS_ARC_SESSION_ARC_PROVISIONING_RESULT_H_

#include <ostream>

#include "components/arc/mojom/auth.mojom.h"
#include "components/arc/session/arc_stop_reason.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

enum class ProvisioningResultUMA : int;

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

  // Returns true if signin_result from ARC is present.
  bool has_sign_in_result() const;

  // Returns the result of provisioning from inside ARC.
  const mojom::ArcSignInResult* sign_in_result() const;

  // Returns true if signin_result is present with an error.
  bool has_sign_in_error() const;

  // Returns the error of signin_result coming from ARC.
  const mojom::ArcSignInError* sign_in_error() const;

  // Returns true if result has given general sign-in error.
  bool has_general_error(mojom::GeneralSignInError error) const;

  // Returns true if provisioning was successful.
  bool is_success() const;

  // Returns true if ARC provisioning was stopped pre-maturely.
  bool is_stopped() const;

  // Returns the reason for ARC stopped event.
  ArcStopReason stop_reason() const;

  // Returns true if ARC provisioning timed out in Chrome.
  bool is_timedout() const;

 private:
  absl::variant<mojom::ArcSignInResultPtr,
                ArcStopReason,
                ChromeProvisioningTimeout>
      result_;
};

// Outputs the stringified |result| to |os|. This is only for logging purposes.
std::ostream& operator<<(std::ostream& os, const ArcProvisioningResult& result);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_SESSION_ARC_PROVISIONING_RESULT_H_

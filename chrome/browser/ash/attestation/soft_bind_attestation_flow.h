// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_SOFT_BIND_ATTESTATION_FLOW_H_
#define CHROME_BROWSER_ASH_ATTESTATION_SOFT_BIND_ATTESTATION_FLOW_H_
#include <string>

#include "base/functional/callback.h"
#include "components/account_id/account_id.h"

namespace ash::attestation {

class SoftBindAttestationFlow {
 public:
  using Callback =
      base::OnceCallback<void(const std::vector<std::string>& certs,
                              bool valid)>;

  SoftBindAttestationFlow() = default;
  virtual ~SoftBindAttestationFlow() = default;

  // !!! WARNING !!! This API should only be called by the browser itself.
  // Any new usage of this API should undergo security review.
  // Must be invoked on the UI thread due to AttestationClient requirements.
  // If the call times out before request completion, the request will
  // continue in the background so long as this object is not freed.
  virtual void GetCertificate(Callback callback,
                              const AccountId& account_id,
                              const std::string& user_key) = 0;
};
}  // namespace ash::attestation
#endif  // CHROME_BROWSER_ASH_ATTESTATION_SOFT_BIND_ATTESTATION_FLOW_H_

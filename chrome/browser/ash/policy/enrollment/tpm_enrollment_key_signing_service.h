// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_TPM_ENROLLMENT_KEY_SIGNING_SERVICE_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_TPM_ENROLLMENT_KEY_SIGNING_SERVICE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "components/policy/core/common/cloud/signing_service.h"

namespace policy {

// Signing class implementing the `SigningService` interface to
// sign data using the enrollment certificate's TPM-bound key.
class TpmEnrollmentKeySigningService : public SigningService {
 public:
  TpmEnrollmentKeySigningService();
  ~TpmEnrollmentKeySigningService() override;

  TpmEnrollmentKeySigningService(const TpmEnrollmentKeySigningService&) =
      delete;
  TpmEnrollmentKeySigningService& operator=(
      const TpmEnrollmentKeySigningService&) = delete;

  void SignData(const std::string& data, SigningCallback callback) override;

 private:
  void OnDataSigned(const std::string& data,
                    SigningCallback callback,
                    const ::attestation::SignSimpleChallengeReply& reply);

  // Used to create tasks which run delayed on the UI thread.
  base::WeakPtrFactory<TpmEnrollmentKeySigningService> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_TPM_ENROLLMENT_KEY_SIGNING_SERVICE_H_

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_ATTESTATION_POLICY_OBSERVER_H_
#define CHROME_BROWSER_ASH_ATTESTATION_ATTESTATION_POLICY_OBSERVER_H_

#include "base/functional/callback.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"

namespace ash {
namespace attestation {

class MachineCertificateUploader;

// A class which observes policy changes and uploads a certificate if necessary.
class AttestationPolicyObserver {
 public:
  // The observer immediately connects with CrosSettings to listen for policy
  // changes.  The CertificateUploader is used to obtain and upload a
  // certificate. This class does not take ownership of |certificate_uploader|.
  explicit AttestationPolicyObserver(
      MachineCertificateUploader* certificate_uploader);

  AttestationPolicyObserver(const AttestationPolicyObserver&) = delete;
  AttestationPolicyObserver& operator=(const AttestationPolicyObserver&) =
      delete;

  ~AttestationPolicyObserver();

 private:
  // Called when the attestation setting changes.
  void AttestationSettingChanged();

  // Checks attestation policy and starts any necessary work.
  void Start();

  CrosSettings* cros_settings_;
  MachineCertificateUploader* certificate_uploader_;

  base::CallbackListSubscription attestation_subscription_;
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_ATTESTATION_POLICY_OBSERVER_H_

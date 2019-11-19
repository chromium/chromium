// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_ATTESTATION_POLICY_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_ATTESTATION_POLICY_OBSERVER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/dbus/constants/attestation_constants.h"

namespace chromeos {
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

  ~AttestationPolicyObserver();

 private:
  // Called when the attestation setting changes.
  void AttestationSettingChanged();

  // Checks attestation policy and starts any necessary work.
  void Start();

  CrosSettings* cros_settings_;
  MachineCertificateUploader* certificate_uploader_;

  std::unique_ptr<CrosSettings::ObserverSubscription> attestation_subscription_;

  DISALLOW_COPY_AND_ASSIGN(AttestationPolicyObserver);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_ATTESTATION_POLICY_OBSERVER_H_

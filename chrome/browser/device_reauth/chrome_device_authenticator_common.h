// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_CHROME_DEVICE_AUTHENTICATOR_COMMON_H_
#define CHROME_BROWSER_DEVICE_REAUTH_CHROME_DEVICE_AUTHENTICATOR_COMMON_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "components/device_reauth/device_authenticator.h"

// Used to care of the auth validity period for biometric authenticators in
// chrome.
class ChromeDeviceAuthenticatorCommon
    : public device_reauth::DeviceAuthenticator {
 public:
  explicit ChromeDeviceAuthenticatorCommon(DeviceAuthenticatorProxy* proxy);

  // Returns a weak pointer to this authenticator.
  base::WeakPtr<ChromeDeviceAuthenticatorCommon> GetWeakPtr();

 protected:
  ~ChromeDeviceAuthenticatorCommon() override;

  // Checks whether user needs to reauthenticate.
  bool NeedsToAuthenticate() const;

  // Records the authentication time if the authentication was successful.
  void RecordAuthenticationTimeIfSuccessful(bool success);

 private:
  // Used to obtain/update the last successful authentication timestamp.
  base::WeakPtr<DeviceAuthenticatorProxy> device_authenticator_proxy_;

  // Factory for weak pointers to this class.
  base::WeakPtrFactory<ChromeDeviceAuthenticatorCommon> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_CHROME_DEVICE_AUTHENTICATOR_COMMON_H_

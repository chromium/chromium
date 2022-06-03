// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_CHROME_BIOMETRIC_AUTHENTICATOR_FACTORY_H_
#define CHROME_BROWSER_DEVICE_REAUTH_CHROME_BIOMETRIC_AUTHENTICATOR_FACTORY_H_

#include "base/memory/scoped_refptr.h"

namespace device_reauth {
class BiometricAuthenticator;
}

// Subclasses are expected to provide an implementation for
// GetBiometricAuthenticator(), instantiating authenticators for a given
// platform. They would also be responsible for deciding whether to create
// multiple/single instances of BiometricAuthenticator.
class ChromeBiometricAuthenticatorFactory {
 public:
  // Get or create an instance of the BiometricAuthenticator. Trying to use this
  // API on platforms that do not provide an implementation will result in a
  // link error. So far only Android provides an implementation.
  static scoped_refptr<device_reauth::BiometricAuthenticator>
  GetBiometricAuthenticator();
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_CHROME_BIOMETRIC_AUTHENTICATOR_FACTORY_H_

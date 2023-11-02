// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_CHROME_BIOMETRIC_AUTHENTICATOR_FACTORY_H_
#define CHROME_BROWSER_DEVICE_REAUTH_CHROME_BIOMETRIC_AUTHENTICATOR_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "components/device_reauth/biometric_authenticator.h"

// Implementation for every OS will be in the same file, as the only thing
// different will be the way of creating a BiometricAuthenticator object, and
// that part will be hidden behind a BUILDFLAG.
class ChromeBiometricAuthenticatorFactory {
 public:
  ChromeBiometricAuthenticatorFactory(
      const ChromeBiometricAuthenticatorFactory& other) = delete;
  ChromeBiometricAuthenticatorFactory& operator=(
      const ChromeBiometricAuthenticatorFactory&) = delete;

  // Get or create an instance of the BiometricAuthenticator. Trying to use this
  // API on platforms that do not provide an implementation will result in a
  // link error. So far only Android provides an implementation.
  // TODO(crbug.com/1349717): Change way of obtaining BiometricAuthenticator
  // from factory.
  static scoped_refptr<device_reauth::BiometricAuthenticator>
  GetBiometricAuthenticator();

  static ChromeBiometricAuthenticatorFactory* GetInstance();

  scoped_refptr<device_reauth::BiometricAuthenticator>
  GetOrCreateBiometricAuthenticator();

 private:
  friend class base::NoDestructor<ChromeBiometricAuthenticatorFactory>;

  ChromeBiometricAuthenticatorFactory();

  ~ChromeBiometricAuthenticatorFactory();

  // The BiometricAuthenticator instance which holds the actual logic for
  // re-authentication. This factory is responsible for creating this instance.
  // Clients can get access to it via
  // ChromeBiometricAuthenticatorFactory::GetBiometricAuthenticator method.
  // Factory doesn't own that object so if there are no references to it and
  // more than 60 seconds have passed since last successful authentication,
  // the authenticator will be destroyed.
  base::WeakPtr<device_reauth::BiometricAuthenticator> biometric_authenticator_;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_CHROME_BIOMETRIC_AUTHENTICATOR_FACTORY_H_

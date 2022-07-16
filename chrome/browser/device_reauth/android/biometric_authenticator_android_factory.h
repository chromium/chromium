// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_ANDROID_BIOMETRIC_AUTHENTICATOR_ANDROID_FACTORY_H_
#define CHROME_BROWSER_DEVICE_REAUTH_ANDROID_BIOMETRIC_AUTHENTICATOR_ANDROID_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/device_reauth/chrome_biometric_authenticator_factory.h"

// A singleton factory that is responsible for creating and providing a shared
// instance of BiometricAuthenticator in Android.
class BiometricAuthenticatorAndroidFactory
    : public ChromeBiometricAuthenticatorFactory {
 public:
  BiometricAuthenticatorAndroidFactory(
      const BiometricAuthenticatorAndroidFactory& other) = delete;
  BiometricAuthenticatorAndroidFactory& operator=(
      const BiometricAuthenticatorAndroidFactory&) = delete;
  ~BiometricAuthenticatorAndroidFactory();

  static BiometricAuthenticatorAndroidFactory* GetInstance();

  scoped_refptr<device_reauth::BiometricAuthenticator>
  GetOrCreateBiometricAuthenticator();

 private:
  friend class base::NoDestructor<BiometricAuthenticatorAndroidFactory>;

  BiometricAuthenticatorAndroidFactory();

  // The BiometricAuthenticator instance which holds the actual logic for
  // re-authentication. This factory is responsible for creating and owning this
  // instance. Clients can get access to it via
  // ChromeBiometricAuthenticatorFactory::GetBiometricAuthenticator method.
  scoped_refptr<device_reauth::BiometricAuthenticator> biometric_authenticator_;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_ANDROID_BIOMETRIC_AUTHENTICATOR_ANDROID_FACTORY_H_

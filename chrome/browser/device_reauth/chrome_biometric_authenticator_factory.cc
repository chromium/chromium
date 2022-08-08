// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chrome_biometric_authenticator_factory.h"

#include "base/notreached.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/device_reauth/android/biometric_authenticator_android.h"
#include "chrome/browser/device_reauth/android/biometric_authenticator_bridge_impl.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/browser/device_reauth/mac/biometric_authenticator_mac.h"
#endif

// static
scoped_refptr<device_reauth::BiometricAuthenticator>
ChromeBiometricAuthenticatorFactory::GetBiometricAuthenticator() {
  return ChromeBiometricAuthenticatorFactory::GetInstance()
      ->GetOrCreateBiometricAuthenticator();
}

// static
ChromeBiometricAuthenticatorFactory*
ChromeBiometricAuthenticatorFactory::GetInstance() {
  static base::NoDestructor<ChromeBiometricAuthenticatorFactory> instance;
  return instance.get();
}

scoped_refptr<device_reauth::BiometricAuthenticator>
ChromeBiometricAuthenticatorFactory::GetOrCreateBiometricAuthenticator() {
  if (!biometric_authenticator_) {
#if BUILDFLAG(IS_ANDROID)
    biometric_authenticator_ =
        (new BiometricAuthenticatorAndroid(
             std::make_unique<BiometricAuthenticatorBridgeImpl>()))
            ->GetWeakPtr();
#elif BUILDFLAG(IS_MAC)
    biometric_authenticator_ = (new BiometricAuthenticatorMac())->GetWeakPtr();
#else
    NOTREACHED();
#endif
  }

  return base::WrapRefCounted(biometric_authenticator_.get());
}

ChromeBiometricAuthenticatorFactory::ChromeBiometricAuthenticatorFactory() =
    default;

ChromeBiometricAuthenticatorFactory::~ChromeBiometricAuthenticatorFactory() =
    default;

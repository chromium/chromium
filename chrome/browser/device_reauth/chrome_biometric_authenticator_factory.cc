// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chrome_biometric_authenticator_factory.h"

#include "base/notreached.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/device_reauth/android/biometric_authenticator_android.h"
#include "chrome/browser/device_reauth/android/biometric_authenticator_bridge_impl.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/browser/device_reauth/mac/biometric_authenticator_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "chrome/browser/device_reauth/win/biometric_authenticator_win.h"
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
    auto biometric_authenticator =
        base::WrapRefCounted(new BiometricAuthenticatorAndroid(
            std::make_unique<BiometricAuthenticatorBridgeImpl>()));
#elif BUILDFLAG(IS_MAC)
    auto biometric_authenticator =
        base::WrapRefCounted(new BiometricAuthenticatorMac());
#elif BUILDFLAG(IS_WIN)
    auto biometric_authenticator = base::WrapRefCounted(
        new BiometricAuthenticatorWin(std::make_unique<AuthenticatorWin>()));
#else
    static_assert(false);
#endif
    biometric_authenticator_ = biometric_authenticator->GetWeakPtr();
    return biometric_authenticator;
  }

  return base::WrapRefCounted(biometric_authenticator_.get());
}

ChromeBiometricAuthenticatorFactory::ChromeBiometricAuthenticatorFactory() {
#if BUILDFLAG(IS_WIN)
  // BiometricAuthenticatorWin is created here only to cache the biometric
  // availability and die. If cached value is wrong(eg. user disable biometrics
  // while chrome is running) then standard password prompt will appear.
  base::WrapRefCounted(
      new BiometricAuthenticatorWin(std::make_unique<AuthenticatorWin>()))
      ->CacheIfBiometricsAvailable();
#endif
}

ChromeBiometricAuthenticatorFactory::~ChromeBiometricAuthenticatorFactory() =
    default;

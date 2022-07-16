// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/android/biometric_authenticator_android_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"

#include "chrome/browser/device_reauth/android/biometric_authenticator_android.h"
#include "chrome/browser/device_reauth/android/biometric_authenticator_bridge_impl.h"
#include "chrome/browser/device_reauth/chrome_biometric_authenticator_factory.h"

// static
scoped_refptr<device_reauth::BiometricAuthenticator>
ChromeBiometricAuthenticatorFactory::GetBiometricAuthenticator() {
  return BiometricAuthenticatorAndroidFactory::GetInstance()
      ->GetOrCreateBiometricAuthenticator();
}

BiometricAuthenticatorAndroidFactory::~BiometricAuthenticatorAndroidFactory() =
    default;

// static
BiometricAuthenticatorAndroidFactory*
BiometricAuthenticatorAndroidFactory::GetInstance() {
  static base::NoDestructor<BiometricAuthenticatorAndroidFactory> instance;
  return instance.get();
}

scoped_refptr<device_reauth::BiometricAuthenticator>
BiometricAuthenticatorAndroidFactory::GetOrCreateBiometricAuthenticator() {
  if (!biometric_authenticator_) {
    biometric_authenticator_ =
        base::WrapRefCounted(new BiometricAuthenticatorAndroid(
            std::make_unique<BiometricAuthenticatorBridgeImpl>()));
  }
  return biometric_authenticator_;
}

BiometricAuthenticatorAndroidFactory::BiometricAuthenticatorAndroidFactory() =
    default;

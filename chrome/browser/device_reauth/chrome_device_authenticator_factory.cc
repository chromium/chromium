// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"

#include "base/notreached.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/device_reauth/android/device_authenticator_android.h"
#include "chrome/browser/device_reauth/android/device_authenticator_bridge_impl.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/browser/device_reauth/mac/authenticator_mac.h"
#include "chrome/browser/device_reauth/mac/device_authenticator_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "chrome/browser/device_reauth/win/device_authenticator_win.h"
#endif

// static
scoped_refptr<device_reauth::DeviceAuthenticator>
ChromeDeviceAuthenticatorFactory::GetDeviceAuthenticator() {
  return ChromeDeviceAuthenticatorFactory::GetInstance()
      ->GetOrCreateDeviceAuthenticator();
}

// static
ChromeDeviceAuthenticatorFactory*
ChromeDeviceAuthenticatorFactory::GetInstance() {
  static base::NoDestructor<ChromeDeviceAuthenticatorFactory> instance;
  return instance.get();
}

scoped_refptr<device_reauth::DeviceAuthenticator>
ChromeDeviceAuthenticatorFactory::GetOrCreateDeviceAuthenticator() {
  if (!biometric_authenticator_) {
#if BUILDFLAG(IS_ANDROID)
    auto biometric_authenticator =
        base::WrapRefCounted(new DeviceAuthenticatorAndroid(
            std::make_unique<DeviceAuthenticatorBridgeImpl>()));
#elif BUILDFLAG(IS_MAC)
    auto biometric_authenticator = base::WrapRefCounted(
        new DeviceAuthenticatorMac(std::make_unique<AuthenticatorMac>()));
#elif BUILDFLAG(IS_WIN)
    auto biometric_authenticator = base::WrapRefCounted(
        new DeviceAuthenticatorWin(std::make_unique<AuthenticatorWin>()));
#else
    static_assert(false);
#endif
    biometric_authenticator_ = biometric_authenticator->GetWeakPtr();
    return biometric_authenticator;
  }

  return base::WrapRefCounted(biometric_authenticator_.get());
}

ChromeDeviceAuthenticatorFactory::ChromeDeviceAuthenticatorFactory() {
#if BUILDFLAG(IS_WIN)
  // DeviceAuthenticatorWin is created here only to cache the biometric
  // availability and die. If cached value is wrong(eg. user disable biometrics
  // while chrome is running) then standard password prompt will appear.
  base::WrapRefCounted(
      new DeviceAuthenticatorWin(std::make_unique<AuthenticatorWin>()))
      ->CacheIfBiometricsAvailable();
#endif
}

ChromeDeviceAuthenticatorFactory::~ChromeDeviceAuthenticatorFactory() = default;

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "components/device_reauth/device_authenticator_common.h"
#include "content/public/browser/network_service_instance.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/device_reauth/android/device_authenticator_android.h"
#include "chrome/browser/device_reauth/android/device_authenticator_bridge_impl.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/browser/device_reauth/mac/authenticator_mac.h"
#include "chrome/browser/device_reauth/mac/device_authenticator_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "chrome/browser/device_reauth/win/device_authenticator_win.h"
#elif BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/device_reauth/chromeos/device_authenticator_chromeos.h"
#endif

using content::BrowserContext;
using device_reauth::DeviceAuthenticator;

ChromeDeviceAuthenticatorFactory::ChromeDeviceAuthenticatorFactory()
    : ProfileKeyedServiceFactory(
          "ChromeDeviceAuthenticator",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

ChromeDeviceAuthenticatorFactory::~ChromeDeviceAuthenticatorFactory() = default;

// static
ChromeDeviceAuthenticatorFactory*
ChromeDeviceAuthenticatorFactory::GetInstance() {
  static base::NoDestructor<ChromeDeviceAuthenticatorFactory> instance;
  return instance.get();
}

// static
std::unique_ptr<DeviceAuthenticator>
ChromeDeviceAuthenticatorFactory::GetForProfile(
    Profile* profile,
    const gfx::NativeWindow window,
    const device_reauth::DeviceAuthParams& params) {
  DeviceAuthenticatorProxy* proxy = static_cast<DeviceAuthenticatorProxy*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));

  CHECK(proxy);

#if BUILDFLAG(IS_ANDROID)
  auto device_authenticator = std::make_unique<DeviceAuthenticatorAndroid>(
      std::make_unique<DeviceAuthenticatorBridgeImpl>(window), proxy, params);
#elif BUILDFLAG(IS_MAC)
  auto device_authenticator = std::make_unique<DeviceAuthenticatorMac>(
      std::make_unique<AuthenticatorMac>(), proxy, params);
#elif BUILDFLAG(IS_WIN)
  auto device_authenticator = std::make_unique<DeviceAuthenticatorWin>(
      std::make_unique<AuthenticatorWin>(), proxy, params);
#elif BUILDFLAG(IS_CHROMEOS)
  auto device_authenticator = std::make_unique<DeviceAuthenticatorChromeOS>(
      std::make_unique<AuthenticatorChromeOS>(), proxy, params);
#else
  static_assert(false);
#endif
  return std::move(device_authenticator);
}

#if BUILDFLAG(IS_ANDROID)
// static
std::unique_ptr<device_reauth::DeviceAuthenticator>
ChromeDeviceAuthenticatorFactory::GetForProfile(
    Profile* profile,
    const base::android::JavaParamRef<jobject>& activity,
    const device_reauth::DeviceAuthParams& params) {
  DeviceAuthenticatorProxy* proxy = static_cast<DeviceAuthenticatorProxy*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));

  CHECK(proxy);

  return std::make_unique<DeviceAuthenticatorAndroid>(
      std::make_unique<DeviceAuthenticatorBridgeImpl>(activity), proxy, params);
}
#endif

std::unique_ptr<KeyedService>
ChromeDeviceAuthenticatorFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {
#if BUILDFLAG(IS_WIN)
  // Here we cache the biometric availability. If cached value is wrong(eg. user
  // disable biometrics while chrome is running) then standard password prompt
  // will appear.
  DeviceAuthenticatorWin::CacheIfBiometricsAvailable(
      std::make_unique<AuthenticatorWin>().get());
#endif

  return std::make_unique<DeviceAuthenticatorProxy>();
}

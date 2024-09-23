// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_CHROME_DEVICE_AUTHENTICATOR_FACTORY_H_
#define CHROME_BROWSER_DEVICE_REAUTH_CHROME_DEVICE_AUTHENTICATOR_FACTORY_H_

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/device_reauth/device_authenticator.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class BrowserContext;
}

// Implementation for every OS will be in the same file, as the only thing
// different will be the way of creating a DeviceAuthenticator object, and
// that part will be hidden behind a BUILDFLAG.
class ChromeDeviceAuthenticatorFactory : public ProfileKeyedServiceFactory {
 public:
  ChromeDeviceAuthenticatorFactory(
      const ChromeDeviceAuthenticatorFactory& other) = delete;
  ChromeDeviceAuthenticatorFactory& operator=(
      const ChromeDeviceAuthenticatorFactory&) = delete;

  static ChromeDeviceAuthenticatorFactory* GetInstance();

  // Create an instance of the DeviceAuthenticator. Trying to use this
  // API on platforms that do not provide an implementation will result in a
  // link error.
  static std::unique_ptr<device_reauth::DeviceAuthenticator> GetForProfile(
      Profile* profile,
      const gfx::NativeWindow window,
      const device_reauth::DeviceAuthParams& params);

#if BUILDFLAG(IS_ANDROID)
  // Create an instance of the DeviceAuthenticator. Trying to use this
  // API on platforms that do not provide an implementation will result in a
  // link error.
  static std::unique_ptr<device_reauth::DeviceAuthenticator> GetForProfile(
      Profile* profile,
      const base::android::JavaParamRef<jobject>& activity,
      const device_reauth::DeviceAuthParams& params);
#endif

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  friend class base::NoDestructor<ChromeDeviceAuthenticatorFactory>;

  ChromeDeviceAuthenticatorFactory();

  ~ChromeDeviceAuthenticatorFactory() override;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_CHROME_DEVICE_AUTHENTICATOR_FACTORY_H_

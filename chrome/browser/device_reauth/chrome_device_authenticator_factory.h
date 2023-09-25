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
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}

// Helper class which keeps the last good authentication timestamp such that it
// is common per profile.
class DeviceAuthenticatorProxy : public KeyedService {
 public:
  DeviceAuthenticatorProxy();
  ~DeviceAuthenticatorProxy() override;

  absl::optional<base::TimeTicks> GetLastGoodAuthTimestamp() {
    return last_good_auth_timestamp_;
  }
  void UpdateLastGoodAuthTimestamp() {
    last_good_auth_timestamp_ = base::TimeTicks::Now();
  }
  base::WeakPtr<DeviceAuthenticatorProxy> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Time of last successful re-auth. nullopt if there hasn't been an auth yet.
  absl::optional<base::TimeTicks> last_good_auth_timestamp_;

  // Factory for weak pointers to this class.
  base::WeakPtrFactory<DeviceAuthenticatorProxy> weak_ptr_factory_{this};
};

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
      const device_reauth::DeviceAuthParams& params);

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  friend class base::NoDestructor<ChromeDeviceAuthenticatorFactory>;

  ChromeDeviceAuthenticatorFactory();

  ~ChromeDeviceAuthenticatorFactory() override;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_CHROME_DEVICE_AUTHENTICATOR_FACTORY_H_

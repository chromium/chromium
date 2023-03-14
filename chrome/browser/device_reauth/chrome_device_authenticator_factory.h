// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_CHROME_DEVICE_AUTHENTICATOR_FACTORY_H_
#define CHROME_BROWSER_DEVICE_REAUTH_CHROME_DEVICE_AUTHENTICATOR_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "components/device_reauth/device_authenticator.h"

// Implementation for every OS will be in the same file, as the only thing
// different will be the way of creating a DeviceAuthenticator object, and
// that part will be hidden behind a BUILDFLAG.
class ChromeDeviceAuthenticatorFactory {
 public:
  ChromeDeviceAuthenticatorFactory(
      const ChromeDeviceAuthenticatorFactory& other) = delete;
  ChromeDeviceAuthenticatorFactory& operator=(
      const ChromeDeviceAuthenticatorFactory&) = delete;

  // Get or create an instance of the DeviceAuthenticator. Trying to use this
  // API on platforms that do not provide an implementation will result in a
  // link error. So far only Android provides an implementation.
  // TODO(crbug.com/1349717): Change way of obtaining DeviceAuthenticator
  // from factory.
  static scoped_refptr<device_reauth::DeviceAuthenticator>
  GetDeviceAuthenticator();

  static ChromeDeviceAuthenticatorFactory* GetInstance();

  scoped_refptr<device_reauth::DeviceAuthenticator>
  GetOrCreateDeviceAuthenticator();

 private:
  friend class base::NoDestructor<ChromeDeviceAuthenticatorFactory>;

  ChromeDeviceAuthenticatorFactory();

  ~ChromeDeviceAuthenticatorFactory();

  // The DeviceAuthenticator instance which holds the actual logic for
  // re-authentication. This factory is responsible for creating this instance.
  // Clients can get access to it via
  // ChromeDeviceAuthenticatorFactory::GetDeviceAuthenticator method.
  // Factory doesn't own that object so if there are no references to it and
  // more than 60 seconds have passed since last successful authentication,
  // the authenticator will be destroyed.
  base::WeakPtr<device_reauth::DeviceAuthenticator> biometric_authenticator_;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_CHROME_DEVICE_AUTHENTICATOR_FACTORY_H_

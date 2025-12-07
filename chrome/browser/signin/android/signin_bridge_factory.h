// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ANDROID_SIGNIN_BRIDGE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_ANDROID_SIGNIN_BRIDGE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class Profile;
class SigninBridge;

// Factory class for SigninBridge.
class SigninBridgeFactory : public ProfileKeyedServiceFactory {
 public:
  static SigninBridge* GetForProfile(Profile* profile);
  static SigninBridgeFactory* GetInstance();

 private:
  friend base::NoDestructor<SigninBridgeFactory>;

  SigninBridgeFactory();

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SIGNIN_ANDROID_SIGNIN_BRIDGE_FACTORY_H_

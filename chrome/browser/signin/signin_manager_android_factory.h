// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_ANDROID_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_ANDROID_FACTORY_H_

#include "base/android/scoped_java_ref.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class SigninManagerAndroid;

class SigninManagerAndroidFactory : public ProfileKeyedServiceFactory {
 public:
  static SigninManagerAndroid* GetForProfile(Profile* profile);

  // Returns an instance of the SigninManagerAndroidFactory singleton.
  static SigninManagerAndroidFactory* GetInstance();

 private:
  friend class base::NoDestructor<SigninManagerAndroidFactory>;
  SigninManagerAndroidFactory();

  ~SigninManagerAndroidFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_ANDROID_FACTORY_H_

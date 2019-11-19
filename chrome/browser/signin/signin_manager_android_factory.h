// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_ANDROID_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_ANDROID_FACTORY_H_

#include "base/android/scoped_java_ref.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

class SigninManagerAndroidFactory : public BrowserContextKeyedServiceFactory {
 public:
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObjectForProfile(
      Profile* profile);

  // Returns an instance of the SigninManagerAndroidFactory singleton.
  static SigninManagerAndroidFactory* GetInstance();

 private:
  friend class base::NoDestructor<SigninManagerAndroidFactory>;
  SigninManagerAndroidFactory();

  ~SigninManagerAndroidFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_ANDROID_FACTORY_H_

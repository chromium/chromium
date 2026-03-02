// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_ANDROID_PROFILE_BROWSER_COLLECTION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_ANDROID_ANDROID_PROFILE_BROWSER_COLLECTION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

#if !BUILDFLAG(IS_ANDROID)
#error This file should only be included on Android.
#endif

class AndroidProfileBrowserCollectionService;
class Profile;

class AndroidProfileBrowserCollectionServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AndroidProfileBrowserCollectionService* GetForProfile(
      Profile* profile);
  static AndroidProfileBrowserCollectionServiceFactory* GetInstance();

  AndroidProfileBrowserCollectionServiceFactory(
      const AndroidProfileBrowserCollectionServiceFactory&) = delete;
  AndroidProfileBrowserCollectionServiceFactory& operator=(
      const AndroidProfileBrowserCollectionServiceFactory&) = delete;

 private:
  friend base::NoDestructor<AndroidProfileBrowserCollectionServiceFactory>;

  AndroidProfileBrowserCollectionServiceFactory();
  ~AndroidProfileBrowserCollectionServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_ANDROID_ANDROID_PROFILE_BROWSER_COLLECTION_SERVICE_FACTORY_H_

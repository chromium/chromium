// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class BackgroundFetchDelegateImpl;
class Profile;

class BackgroundFetchDelegateFactory : public ProfileKeyedServiceFactory {
 public:
  static BackgroundFetchDelegateImpl* GetForProfile(Profile* profile);
  static BackgroundFetchDelegateFactory* GetInstance();

  BackgroundFetchDelegateFactory(const BackgroundFetchDelegateFactory&) =
      delete;
  BackgroundFetchDelegateFactory& operator=(
      const BackgroundFetchDelegateFactory&) = delete;

 private:
  friend base::NoDestructor<BackgroundFetchDelegateFactory>;

  BackgroundFetchDelegateFactory();
  ~BackgroundFetchDelegateFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_FACTORY_H_

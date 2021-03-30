// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_ANDROID_INCOGNITO_SESSION_DURATIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ANDROID_METRICS_ANDROID_INCOGNITO_SESSION_DURATIONS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class AndroidIncognitoSessionDurationsService;
class Profile;

class AndroidIncognitoSessionDurationsServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // If there is an active user profile and it has an Incognito profile, then
  // it returns the service associated with the Incognito profile (it creates
  // the service if it does not exist already).
  static AndroidIncognitoSessionDurationsService* GetForActiveUserProfile();

  // Creates the service if it doesn't exist already for the given
  // BrowserContext. If the BrowserContext is not an Incognito one, nullptr is
  // returned.
  static AndroidIncognitoSessionDurationsService* GetForProfile(
      Profile* profile);

  static AndroidIncognitoSessionDurationsServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      AndroidIncognitoSessionDurationsServiceFactory>;

  AndroidIncognitoSessionDurationsServiceFactory();
  ~AndroidIncognitoSessionDurationsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  DISALLOW_COPY_AND_ASSIGN(AndroidIncognitoSessionDurationsServiceFactory);
};

#endif  // CHROME_BROWSER_ANDROID_METRICS_ANDROID_INCOGNITO_SESSION_DURATIONS_SERVICE_FACTORY_H_

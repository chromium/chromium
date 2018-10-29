// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_FACTORY_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class GoogleURLTracker;
class Profile;

// Singleton that owns all GoogleURLTrackers and associates them with Profiles.
class GoogleURLTrackerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the GoogleURLTracker for |profile|.  This may return NULL for a
  // testing profile.
  static GoogleURLTracker* GetForProfile(Profile* profile);

  static GoogleURLTrackerFactory* GetInstance();

  // Returns the default factory used to build GoogleURLTracker. Can be
  // registered with SetTestingFactory to use a real GoogleURLTracker instance
  // for testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend struct base::DefaultSingletonTraits<GoogleURLTrackerFactory>;
  friend class GoogleURLTrackerFactoryTest;

  GoogleURLTrackerFactory();
  ~GoogleURLTrackerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTrackerFactory);
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_FACTORY_H_

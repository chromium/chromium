// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class MediaEngagementService;
class Profile;

class MediaEngagementServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static MediaEngagementService* GetForProfile(Profile* profile);
  static MediaEngagementServiceFactory* GetInstance();

  MediaEngagementServiceFactory(const MediaEngagementServiceFactory&) = delete;
  MediaEngagementServiceFactory& operator=(
      const MediaEngagementServiceFactory&) = delete;

 private:
  friend base::NoDestructor<MediaEngagementServiceFactory>;

  MediaEngagementServiceFactory();
  ~MediaEngagementServiceFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_FACTORY_H_

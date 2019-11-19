// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class MediaEngagementService;
class Profile;

class MediaEngagementServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static MediaEngagementService* GetForProfile(Profile* profile);
  static MediaEngagementServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<MediaEngagementServiceFactory>;

  MediaEngagementServiceFactory();
  ~MediaEngagementServiceFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(MediaEngagementServiceFactory);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_FACTORY_H_

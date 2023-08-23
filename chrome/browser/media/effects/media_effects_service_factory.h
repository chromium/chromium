// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/media/effects/media_effects_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class MediaEffectsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static MediaEffectsService* GetForProfile(Profile* profile);
  static MediaEffectsServiceFactory* GetInstance();

  MediaEffectsServiceFactory(const MediaEffectsServiceFactory&) = delete;
  MediaEffectsServiceFactory& operator=(const MediaEffectsServiceFactory&) =
      delete;

  MediaEffectsServiceFactory(MediaEffectsServiceFactory&&) = delete;
  MediaEffectsServiceFactory& operator=(MediaEffectsServiceFactory&&) = delete;

 private:
  friend base::NoDestructor<MediaEffectsServiceFactory>;

  MediaEffectsServiceFactory();
  ~MediaEffectsServiceFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_FACTORY_H_

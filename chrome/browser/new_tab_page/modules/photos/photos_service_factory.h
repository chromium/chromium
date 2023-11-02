// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_PHOTOS_PHOTOS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_PHOTOS_PHOTOS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class PhotosService;
class Profile;

class PhotosServiceFactory : ProfileKeyedServiceFactory {
 public:
  static PhotosService* GetForProfile(Profile* profile);
  static PhotosServiceFactory* GetInstance();
  PhotosServiceFactory(const PhotosServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<PhotosServiceFactory>;
  PhotosServiceFactory();
  ~PhotosServiceFactory() override;

  // Uses BrowserContextKeyedServiceFactory to build a PhotosService.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_PHOTOS_PHOTOS_SERVICE_FACTORY_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMAGE_SERVICE_IMAGE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_IMAGE_SERVICE_IMAGE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace history_clusters {
class EntityImageService;
}

namespace image_service {

// Factory for BrowserContext keyed ImageService, which provides images for
// Journeys related features.
class ImageServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // This can return nullptr in tests.
  static history_clusters::EntityImageService* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  friend base::NoDestructor<ImageServiceFactory>;
  static ImageServiceFactory& GetInstance();

  ImageServiceFactory();
  ~ImageServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace image_service

#endif  // CHROME_BROWSER_IMAGE_SERVICE_IMAGE_SERVICE_FACTORY_H_

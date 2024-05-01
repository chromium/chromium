// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace page_image_service {

class ImageService;

// Factory for BrowserContext keyed ImageService, which provides images for
// Journeys related features.
class ImageServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // This can return nullptr in tests.
  static ImageService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static void EnsureFactoryBuilt();

 private:
  friend base::NoDestructor<ImageServiceFactory>;
  static ImageServiceFactory& GetInstance();

  ImageServiceFactory();
  ~ImageServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace page_image_service

#endif  // CHROME_BROWSER_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_FACTORY_H_

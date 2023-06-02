// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMAGE_FETCHER_IMAGE_FETCHER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_IMAGE_FETCHER_IMAGE_FETCHER_SERVICE_FACTORY_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

class SimpleFactoryKey;

namespace image_fetcher {
class ImageFetcherService;
}  // namespace image_fetcher

// Factory to create one CachedImageFetcherService per browser context.
class ImageFetcherServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static image_fetcher::ImageFetcherService* GetForKey(SimpleFactoryKey* key);
  static ImageFetcherServiceFactory* GetInstance();

  ImageFetcherServiceFactory(const ImageFetcherServiceFactory&) = delete;
  ImageFetcherServiceFactory& operator=(const ImageFetcherServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<ImageFetcherServiceFactory>;

  ImageFetcherServiceFactory();
  ~ImageFetcherServiceFactory() override;

  // SimpleKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;

  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;
};

#endif  // CHROME_BROWSER_IMAGE_FETCHER_IMAGE_FETCHER_SERVICE_FACTORY_H_

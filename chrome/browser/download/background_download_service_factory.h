// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BACKGROUND_DOWNLOAD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DOWNLOAD_BACKGROUND_DOWNLOAD_SERVICE_FACTORY_H_

#include <memory>

#include "components/keyed_service/core/simple_keyed_service_factory.h"

class SimpleFactoryKey;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace download {
class BackgroundDownloadService;
}  // namespace download

// BackgroundDownloadServiceFactory is the main client class for interaction
// with the download component.
class BackgroundDownloadServiceFactory : public SimpleKeyedServiceFactory {
 public:
  // Returns singleton instance of DownloadServiceFactory.
  static BackgroundDownloadServiceFactory* GetInstance();

  // Returns the DownloadService associated with |key|.
  static download::BackgroundDownloadService* GetForKey(SimpleFactoryKey* key);

  BackgroundDownloadServiceFactory(const BackgroundDownloadServiceFactory&) =
      delete;
  BackgroundDownloadServiceFactory& operator=(
      const BackgroundDownloadServiceFactory&) = delete;

 private:
  friend base::NoDestructor<BackgroundDownloadServiceFactory>;

  BackgroundDownloadServiceFactory();
  ~BackgroundDownloadServiceFactory() override;

  // SimpleKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;
};

#endif  // CHROME_BROWSER_DOWNLOAD_BACKGROUND_DOWNLOAD_SERVICE_FACTORY_H_

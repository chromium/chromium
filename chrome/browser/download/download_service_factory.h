// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

class SimpleFactoryKey;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace download {
class DownloadService;
}  // namespace download

// DownloadServiceFactory is the main client class for interaction with the
// download component.
class DownloadServiceFactory : public SimpleKeyedServiceFactory {
 public:
  // Returns singleton instance of DownloadServiceFactory.
  static DownloadServiceFactory* GetInstance();

  // Returns the DownloadService associated with |key|.
  static download::DownloadService* GetForKey(SimpleFactoryKey* key);

 private:
  friend struct base::DefaultSingletonTraits<DownloadServiceFactory>;

  DownloadServiceFactory();
  ~DownloadServiceFactory() override;

  // SimpleKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;

  DISALLOW_COPY_AND_ASSIGN(DownloadServiceFactory);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_FACTORY_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_SIMPLE_DOWNLOAD_MANAGER_COORDINATOR_FACTORY_H_
#define CHROME_BROWSER_DOWNLOAD_SIMPLE_DOWNLOAD_MANAGER_COORDINATOR_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

class KeyedService;
class SimpleFactoryKey;

namespace download {
class SimpleDownloadManagerCoordinator;
}  // namespace download

// A factory for SimpleDownloadManagerCoordinator. It can be used to create
// the SimpleDownloadManagerCoordinator before full browser process is created.
class SimpleDownloadManagerCoordinatorFactory
    : public SimpleKeyedServiceFactory {
 public:
  // Returns singleton instance of SimpleDownloadManagerCoordinatorFactory.
  static SimpleDownloadManagerCoordinatorFactory* GetInstance();

  // Returns SimpleDownloadManagerCoordinator associated with |key|.
  static download::SimpleDownloadManagerCoordinator* GetForKey(
      SimpleFactoryKey* key);

  SimpleDownloadManagerCoordinatorFactory(
      const SimpleDownloadManagerCoordinatorFactory&) = delete;
  SimpleDownloadManagerCoordinatorFactory& operator=(
      const SimpleDownloadManagerCoordinatorFactory&) = delete;

 private:
  friend class base::NoDestructor<SimpleDownloadManagerCoordinatorFactory>;

  SimpleDownloadManagerCoordinatorFactory();
  ~SimpleDownloadManagerCoordinatorFactory() override;

  // SimpleKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;
};

#endif  // CHROME_BROWSER_DOWNLOAD_SIMPLE_DOWNLOAD_MANAGER_COORDINATOR_FACTORY_H_

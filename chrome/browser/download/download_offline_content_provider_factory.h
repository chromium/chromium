// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OFFLINE_CONTENT_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OFFLINE_CONTENT_PROVIDER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

class DownloadOfflineContentProvider;
class SimpleFactoryKey;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

// This class builds and associates DownloadOfflineContentProvider with their
// Profiles, represented by SimpleFactoryKeys.
class DownloadOfflineContentProviderFactory : public SimpleKeyedServiceFactory {
 public:
  // Returns a singleton instance of an DownloadOfflineContentProviderFactory.
  static DownloadOfflineContentProviderFactory* GetInstance();

  // Returns the DownloadOfflineContentProvider associated with |key| or creates
  // and associates one if it doesn't exist.
  static DownloadOfflineContentProvider* GetForKey(SimpleFactoryKey* key);

 private:
  friend struct base::DefaultSingletonTraits<
      DownloadOfflineContentProviderFactory>;

  DownloadOfflineContentProviderFactory();
  ~DownloadOfflineContentProviderFactory() override;

  // SimpleKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;

  DISALLOW_COPY_AND_ASSIGN(DownloadOfflineContentProviderFactory);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OFFLINE_CONTENT_PROVIDER_FACTORY_H_

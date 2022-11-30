// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_SERVICE_FACTORY_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_SERVICE_FACTORY_H_

#include <memory>

#include "components/keyed_service/core/simple_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace offline_pages {

class PrefetchService;

// A factory to create one PrefetchServiceImpl per browser context. Prefetching
// Offline Pages is not supported in incognito, so this class uses default
// implementation of |GetBrowserContextToUse|.
class PrefetchServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static PrefetchServiceFactory* GetInstance();
  static PrefetchService* GetForKey(SimpleFactoryKey* key);

  PrefetchServiceFactory(const PrefetchServiceFactory&) = delete;
  PrefetchServiceFactory& operator=(const PrefetchServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<PrefetchServiceFactory>;

  PrefetchServiceFactory();
  ~PrefetchServiceFactory() override {}

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_SERVICE_FACTORY_H_

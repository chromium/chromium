// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_ITEMS_COLLECTION_OFFLINE_CONTENT_AGGREGATOR_FACTORY_H_
#define CHROME_BROWSER_OFFLINE_ITEMS_COLLECTION_OFFLINE_CONTENT_AGGREGATOR_FACTORY_H_

#include <memory>

#include "components/keyed_service/core/simple_keyed_service_factory.h"

class SimpleFactoryKey;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace offline_items_collection {
class OfflineContentAggregator;
}  // namespace offline_items_collection

// This class is the main access point for an OfflineContentAggregator.  It is
// responsible for building the OfflineContentAggregator and associating it with
// a particular SimpleFactoryKey.
class OfflineContentAggregatorFactory : public SimpleKeyedServiceFactory {
 public:
  // Returns a singleton instance of an OfflineContentAggregatorFactory.
  static OfflineContentAggregatorFactory* GetInstance();

  // Returns the OfflineContentAggregator associated with |key| or creates and
  // associates one if it doesn't exist.
  static offline_items_collection::OfflineContentAggregator* GetForKey(
      SimpleFactoryKey* key);

  OfflineContentAggregatorFactory(const OfflineContentAggregatorFactory&) =
      delete;
  OfflineContentAggregatorFactory& operator=(
      const OfflineContentAggregatorFactory&) = delete;

 private:
  friend base::NoDestructor<OfflineContentAggregatorFactory>;

  OfflineContentAggregatorFactory();
  ~OfflineContentAggregatorFactory() override;

  // SimpleKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;
};

#endif  // CHROME_BROWSER_OFFLINE_ITEMS_COLLECTION_OFFLINE_CONTENT_AGGREGATOR_FACTORY_H_

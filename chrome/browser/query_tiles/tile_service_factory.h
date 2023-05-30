// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_QUERY_TILES_TILE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_QUERY_TILES_TILE_SERVICE_FACTORY_H_

#include <memory>

#include "components/keyed_service/core/simple_keyed_service_factory.h"
#include "components/query_tiles/tile_service.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace query_tiles {

class TileService;

// A factory to create one unique TileService.
class TileServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static TileServiceFactory* GetInstance();
  static TileService* GetForKey(SimpleFactoryKey* key);

  TileServiceFactory(const TileServiceFactory&) = delete;
  TileServiceFactory& operator=(const TileServiceFactory&) = delete;

 private:
  friend base::NoDestructor<TileServiceFactory>;

  TileServiceFactory();
  ~TileServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
};

}  // namespace query_tiles

#endif  // CHROME_BROWSER_QUERY_TILES_TILE_SERVICE_FACTORY_H_

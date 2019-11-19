// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GAMES_GAMES_SERVICE_FACTORY_H_
#define CHROME_BROWSER_GAMES_GAMES_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "components/games/core/games_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace games {

class GamesServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static GamesServiceFactory* GetInstance();
  static GamesService* GetForBrowserContext(content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<GamesServiceFactory>;

  GamesServiceFactory();
  ~GamesServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(GamesServiceFactory);
};

}  // namespace games

#endif  // CHROME_BROWSER_GAMES_GAMES_SERVICE_FACTORY_H_

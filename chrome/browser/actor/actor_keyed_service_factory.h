// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "base/types/pass_key.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace actor {

// Handles actors within chrome. Only regular, non-OTR profiles are supported.
class ActorKeyedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  explicit ActorKeyedServiceFactory(base::PassKey<ActorKeyedServiceFactory>);
  static ActorKeyedServiceFactory* GetInstance();

  static ActorKeyedService* GetActorKeyedService(
      content::BrowserContext* browser_context);

  ActorKeyedServiceFactory(const ActorKeyedServiceFactory&) = delete;
  ActorKeyedServiceFactory& operator=(const ActorKeyedServiceFactory&) = delete;

  // BrowserContextKeyedServiceFactory implementation:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

 private:
  ~ActorKeyedServiceFactory() override;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_FACTORY_H_

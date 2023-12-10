// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_REGISTRY_FACTORY_H_
#define CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_REGISTRY_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
namespace custom_handlers {
class ProtocolHandlerRegistry;
}

namespace base {
template <typename T>
class NoDestructor;
}

// Singleton that owns all ProtocolHandlerRegistrys and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated ProtocolHandlerRegistry.
class ProtocolHandlerRegistryFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the singleton instance of the ProtocolHandlerRegistryFactory.
  static ProtocolHandlerRegistryFactory* GetInstance();

  // Returns the ProtocolHandlerRegistry that provides intent registration for
  // |context|. Ownership stays with this factory object.
  static custom_handlers::ProtocolHandlerRegistry* GetForBrowserContext(
      content::BrowserContext* context);

  ProtocolHandlerRegistryFactory(const ProtocolHandlerRegistryFactory&) =
      delete;
  ProtocolHandlerRegistryFactory& operator=(
      const ProtocolHandlerRegistryFactory&) = delete;

 protected:
  // BrowserContextKeyedServiceFactory implementation.
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend base::NoDestructor<ProtocolHandlerRegistryFactory>;

  ProtocolHandlerRegistryFactory();
  ~ProtocolHandlerRegistryFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_REGISTRY_FACTORY_H_

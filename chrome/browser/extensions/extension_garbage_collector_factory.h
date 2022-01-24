// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace extensions {

class ExtensionGarbageCollector;

class ExtensionGarbageCollectorFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  ExtensionGarbageCollectorFactory(const ExtensionGarbageCollectorFactory&) =
      delete;
  ExtensionGarbageCollectorFactory& operator=(
      const ExtensionGarbageCollectorFactory&) = delete;

  static ExtensionGarbageCollector* GetForBrowserContext(
      content::BrowserContext* context);

  static ExtensionGarbageCollectorFactory* GetInstance();

  static std::unique_ptr<KeyedService> BuildInstanceFor(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<ExtensionGarbageCollectorFactory>;

  ExtensionGarbageCollectorFactory();
  ~ExtensionGarbageCollectorFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_GARBAGE_COLLECTOR_FACTORY_H_

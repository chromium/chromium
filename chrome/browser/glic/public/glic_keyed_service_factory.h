// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_KEYED_SERVICE_FACTORY_H_

#include <memory>

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace glic {

class GlicKeyedService;

class GlicKeyedServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static GlicKeyedServiceFactory* GetInstance();

  static GlicKeyedService* GetGlicKeyedService(
      content::BrowserContext* browser_context,
      bool create = false);

  GlicKeyedServiceFactory(const GlicKeyedServiceFactory&) = delete;
  GlicKeyedServiceFactory& operator=(const GlicKeyedServiceFactory&) = delete;

  // BrowserContextKeyedServiceFactory implementation:
  bool ServiceIsCreatedWithBrowserContext() const override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const final;

 private:
  friend base::NoDestructor<GlicKeyedServiceFactory>;

  GlicKeyedServiceFactory();
  ~GlicKeyedServiceFactory() override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_KEYED_SERVICE_FACTORY_H_

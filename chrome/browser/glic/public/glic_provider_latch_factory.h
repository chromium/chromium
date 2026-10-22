// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_LATCH_FACTORY_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_LATCH_FACTORY_H_

#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

class KeyedService;

namespace glic {

// Resolves the `GlicProvider` for each regular user profile
// (`ProfileSelections::BuildForRegularProfile()`) when its keyed services are
// created, and stores it in `GlicProviderLatch`. The keyed service itself is an
// empty marker; the value lives in `GlicProviderLatch` so that low-level code
// can read it without depending on this factory.
//
// Factories whose services read the provider during construction must
// `DependsOn(GlicProviderLatchFactory::GetInstance())`.
class GlicProviderLatchFactory : public ProfileKeyedServiceFactory {
 public:
  static GlicProviderLatchFactory* GetInstance();

  // Overrides the provider latched for `context` in place of the value the
  // factory would resolve. Must be called before `context`'s keyed services are
  // created, for example from a
  // `BrowserContextDependencyManager::RegisterCreateServicesCallbackForTesting`
  // callback.
  static void SetProviderForTesting(content::BrowserContext* context,
                                    GlicProvider provider);

 private:
  friend base::NoDestructor<GlicProviderLatchFactory>;

  GlicProviderLatchFactory();
  ~GlicProviderLatchFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_LATCH_FACTORY_H_

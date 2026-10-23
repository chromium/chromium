// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_latch_factory.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/types/pass_key.h"
#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace glic {

// static
GlicProviderLatchFactory* GlicProviderLatchFactory::GetInstance() {
  static base::NoDestructor<GlicProviderLatchFactory> instance;
  return instance.get();
}

// static
void GlicProviderLatchFactory::SetProviderForTesting(
    content::BrowserContext* context,
    GlicProvider provider) {
  GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(
          [](GlicProvider provider,
             content::BrowserContext* ctx) -> std::unique_ptr<KeyedService> {
            GeicProviderInitOptions geic_options;
            if (provider == GlicProvider::kGeminiEnterprise) {
              geic_options.guest_url = geic::ResolveGeicGuestUrl(ctx);
            }
            GlicProviderLatch::Set(base::PassKey<GlicProviderLatchFactory>(),
                                   *ctx, provider, std::move(geic_options));
            return std::make_unique<KeyedService>();
          },
          provider));
}

GlicProviderLatchFactory::GlicProviderLatchFactory()
    : ProfileKeyedServiceFactory("GlicProviderLatch",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(GoogleGroupsManagerFactory::GetInstance());
}

GlicProviderLatchFactory::~GlicProviderLatchFactory() = default;

std::unique_ptr<KeyedService>
GlicProviderLatchFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  GURL geic_guest_url = geic::ResolveGeicGuestUrl(context);
  GlicProvider provider = geic_guest_url.is_empty()
                              ? GlicProvider::kGemini
                              : GlicProvider::kGeminiEnterprise;
  GlicProviderLatch::Set(
      base::PassKey<GlicProviderLatchFactory>(), *context, provider,
      GeicProviderInitOptions{.guest_url = std::move(geic_guest_url)});
  return std::make_unique<KeyedService>();
}

bool GlicProviderLatchFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool GlicProviderLatchFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

}  // namespace glic

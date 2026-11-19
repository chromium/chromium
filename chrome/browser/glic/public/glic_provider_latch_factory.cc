// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_latch_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/types/pass_key.h"
#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_change.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/public/glic_provider_switch_notice.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
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
    GlicProvider provider,
    GeicProviderInitOptions geic_options) {
  // GE profiles are always latched with a valid guest URL.
  CHECK(provider != GlicProvider::kGeminiEnterprise ||
        geic_options.guest_url.is_valid());
  GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(
          [](GlicProvider provider, const GeicProviderInitOptions& geic_options,
             content::BrowserContext* ctx) -> std::unique_ptr<KeyedService> {
            GlicProviderLatch::Set(base::PassKey<GlicProviderLatchFactory>(),
                                   *ctx, provider, geic_options);
            return std::make_unique<KeyedService>();
          },
          provider, std::move(geic_options)));
}

// static
GlicProviderChange GlicProviderLatchFactory::GetPendingProviderChange(
    base::PassKey<GlicEnabling>,
    content::BrowserContext* context) {
  if (!context) {
    return GlicProviderChange::kNone;
  }
  const GlicProviderLatch* latch = GlicProviderLatch::GetIfLatched(*context);
  if (!latch) {
    return GlicProviderChange::kNone;
  }
  const GlicProviderKeyPassKey key_pass_key =
      GlicProviderKeyPasskeyProvider::GetPassKey();
  GlicProviderKey latched_key = latch->GetSpec().GetProviderKey(key_pass_key);
  GlicProviderKey resolved_key = GlicProviderKey::Gemini();
  GURL resolved_url = geic::ResolveGeicGuestUrl(context);
  // Make the same decision as `BuildServiceInstanceForBrowserContext()`, so
  // e.g. a profile that selected consumer Gemini doesn't report a
  // `kGeminiToGeic` change that a relaunch wouldn't apply.
  if (geic::ShouldLatchToGeminiEnterprise(context, resolved_url)) {
    // The key of a GEiC spec latched with `resolved_url`.
    resolved_key =
        CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise,
                               {.guest_url = std::move(resolved_url)})
            ->GetProviderKey(key_pass_key);
  }
  return ClassifyProviderChange(latched_key, resolved_key);
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
  GeicProviderInitOptions geic_options;
  geic_options.can_switch_between_gemini_providers =
      geic::CanSwitchBetweenGeminiProvidersInSettings(context);
  GlicProvider provider;
  if (geic::ShouldLatchToGeminiEnterprise(context, geic_guest_url)) {
    provider = GlicProvider::kGeminiEnterprise;
    geic_options.guest_url = std::move(geic_guest_url);
  } else {
    provider = GlicProvider::kGemini;
  }
  GlicProviderLatch::Set(base::PassKey<GlicProviderLatchFactory>(), *context,
                         provider, std::move(geic_options));
  UpdateKnownProviderAtStartup(base::PassKey<GlicProviderLatchFactory>(),
                               *Profile::FromBrowserContext(context));
  return std::make_unique<KeyedService>();
}

bool GlicProviderLatchFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool GlicProviderLatchFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

}  // namespace glic

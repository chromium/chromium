// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_latch_factory.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/types/pass_key.h"
#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_change.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace glic {

namespace {

// Appended by `GetLocalizedGuestURL()` from Chrome's UI locale.
constexpr char kLocaleParam[] = "hl";

// Returns `guest_url` without `hl`, or std::nullopt if it's invalid or empty.
// TODO(b/578040846): Use the shared guest URL key helper.
std::optional<std::string> GuestUrlToKey(const GURL& guest_url) {
  if (!guest_url.is_valid()) {
    return std::nullopt;
  }
  GURL key =
      net::AppendOrReplaceQueryParameter(guest_url, kLocaleParam, std::nullopt);
  if (key.query().empty()) {
    // Drop the empty "?" left when `hl` was the only param.
    GURL::Replacements replacements;
    replacements.ClearQuery();
    key = key.ReplaceComponents(replacements);
  }
  return key.spec();
}

// Classifies the change from the latched to the currently resolved GEiC guest
// URL. An empty URL means Gemini. URLs are compared without `hl`, like the GEiC
// FRE keys.
GlicProviderChange ClassifyProviderChange(const GURL& latched_geic_url,
                                          const GURL& resolved_geic_url) {
  const std::optional<std::string> latched_key =
      GuestUrlToKey(latched_geic_url);
  const std::optional<std::string> resolved_key =
      GuestUrlToKey(resolved_geic_url);
  if (latched_key == resolved_key) {
    return GlicProviderChange::kNone;
  }
  if (!latched_key) {
    return GlicProviderChange::kGeminiToGeic;
  }
  return resolved_key ? GlicProviderChange::kGeicToOtherGeic
                      : GlicProviderChange::kGeicToGemini;
}

}  // namespace

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
  // For Gemini, the latched GEiC URL is left empty.
  // TODO(b/578040846): Use `GetSpec().GetGuestUrlKey()`.
  const GURL latched_geic_url =
      latch->provider() == GlicProvider::kGeminiEnterprise
          ? latch->GetSpec().GetGuestUrl(Profile::FromBrowserContext(context))
          : GURL();
  return ClassifyProviderChange(latched_geic_url,
                                geic::ResolveGeicGuestUrl(context));
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
  GlicProvider provider;
  if (geic::ShouldLatchToGeminiEnterprise(context, geic_guest_url)) {
    provider = GlicProvider::kGeminiEnterprise;
    geic_options.guest_url = std::move(geic_guest_url);
  } else {
    provider = GlicProvider::kGemini;
  }
  GlicProviderLatch::Set(base::PassKey<GlicProviderLatchFactory>(), *context,
                         provider, std::move(geic_options));
  return std::make_unique<KeyedService>();
}

bool GlicProviderLatchFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool GlicProviderLatchFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

}  // namespace glic

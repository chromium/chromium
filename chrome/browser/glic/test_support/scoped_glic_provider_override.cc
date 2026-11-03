// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/scoped_glic_provider_override.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/glic/public/glic_provider_latch_factory.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/test_support/scoped_geic_test_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace glic {

ScopedGlicProviderOverride::ScopedGlicProviderOverride(GlicProvider provider)
    : ScopedGlicProviderOverride(provider, std::nullopt) {}

ScopedGlicProviderOverride::ScopedGlicProviderOverride(
    GlicProvider provider,
    base::FilePath profile_path)
    : ScopedGlicProviderOverride(
          provider,
          std::optional<base::FilePath>(std::move(profile_path))) {}

ScopedGlicProviderOverride::ScopedGlicProviderOverride(
    GlicProvider provider,
    std::optional<base::FilePath> profile_path)
    : provider_(provider), profile_path_(std::move(profile_path)) {
  // Unretained is safe: the subscription is owned by, and doesn't outlive,
  // `this`.
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating(&ScopedGlicProviderOverride::OnCreateServices,
                                  base::Unretained(this)));
}

ScopedGlicProviderOverride::~ScopedGlicProviderOverride() = default;

void ScopedGlicProviderOverride::OnCreateServices(
    content::BrowserContext* context) {
  if (profile_path_ && context->GetPath() != *profile_path_) {
    return;
  }
  GeicProviderInitOptions geic_options;
  if (provider_ == GlicProvider::kGeminiEnterprise) {
    // GE profiles are always latched with a valid guest URL, which code such
    // as `GlicProviderSpec::GetGuestUrlKey()` relies on. Tests using this
    // override usually don't configure GEiC, so latch a fixed test URL rather
    // than the (possibly empty) configured one.
    geic_options.guest_url = GURL(kTestGeicGuestUrl);
  }
  GlicProviderLatchFactory::SetProviderForTesting(context, provider_,
                                                  std::move(geic_options));
}

}  // namespace glic

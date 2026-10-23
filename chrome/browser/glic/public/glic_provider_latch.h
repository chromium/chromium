// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_LATCH_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_LATCH_H_

#include <memory>

#include "base/no_destructor.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace glic {

class GlicProviderLatchFactory;

// Holds the `GlicProvider` and `GlicProviderSpec` chosen for a profile.
//
// The value is resolved once, eagerly, by `GlicProviderLatchFactory` while the
// profile's keyed services are being created, and never changes for the
// lifetime of the profile. Glic code that needs the provider reads it with
// `GetProvider()`. Code that needs provider-specific values should prefer
// `GetGlicProviderSpec(profile)` over branching on the provider.
//
// Every regular user profile (`ProfileSelections::BuildForRegularProfile()`) is
// latched by `GlicProviderLatchFactory`. If a `BrowserContext` has not been
// latched (e.g. non-regular profiles), `Get()` falls back to a default
// `GlicProvider::kGemini` latch. Keyed service factories that read the
// provider while building their service must
// `DependsOn(GlicProviderLatchFactory::GetInstance())`.
class GlicProviderLatch final : public base::SupportsUserData::Data {
 public:
  GlicProviderLatch(const GlicProviderLatch&) = delete;
  GlicProviderLatch& operator=(const GlicProviderLatch&) = delete;
  ~GlicProviderLatch() override;

  // Returns the `GlicProviderLatch` for `context`, or a fallback
  // `GlicProvider::kGemini` latch if `context` has not been latched.
  static const GlicProviderLatch& Get(content::BrowserContext& context);

  // Returns the latched provider for `context`, or `GlicProvider::kGemini` if
  // `context` has not been latched.
  // TODO(crbug.com/576351868): Consider exposing the appropriate spec or rules
  // directly instead of `GetProvider`.
  static GlicProvider GetProvider(content::BrowserContext& context);

  // Returns the latched `GlicProviderSpec`.
  const GlicProviderSpec& GetSpec() const;

  // Latches `provider` and its `GlicProviderSpec` for `context`. Only
  // `GlicProviderLatchFactory` may call this, once per context.
  static void Set(base::PassKey<GlicProviderLatchFactory>,
                  content::BrowserContext& context,
                  GlicProvider provider,
                  GeicProviderInitOptions geic_options);

 private:
  friend class base::NoDestructor<GlicProviderLatch>;

  GlicProviderLatch(GlicProvider provider,
                    std::unique_ptr<const GlicProviderSpec> spec);

  const GlicProvider provider_;
  const std::unique_ptr<const GlicProviderSpec> spec_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_LATCH_H_

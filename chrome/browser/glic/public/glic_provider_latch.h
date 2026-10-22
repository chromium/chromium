// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_LATCH_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_LATCH_H_

#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "chrome/browser/glic/public/glic_provider.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace glic {

class GlicProviderLatchFactory;

// Holds the `GlicProvider` chosen for a profile.
//
// The value is resolved once, eagerly, by `GlicProviderLatchFactory` while the
// profile's keyed services are being created, and never changes for the
// lifetime of the profile. Glic code that needs the provider reads it with
// `GetProvider()`. Code that needs provider-specific values should prefer
// `GlicKeyedService::provider_spec()` over branching on the provider.
//
// Every regular user profile (`ProfileSelections::BuildForRegularProfile()`) is
// latched by `GlicProviderLatchFactory`. Asking a profile for its provider
// before the latch has been built is a bug, and crashes. Keyed service
// factories that read the provider while building their service must
// `DependsOn(GlicProviderLatchFactory::GetInstance())`.
class GlicProviderLatch final : public base::SupportsUserData::Data {
 public:
  GlicProviderLatch(const GlicProviderLatch&) = delete;
  GlicProviderLatch& operator=(const GlicProviderLatch&) = delete;
  ~GlicProviderLatch() override;

  // Returns the latched provider for `context`, which must be a regular user
  // profile (`profiles::IsRegularUserProfile()`). CHECKs that `context` has
  // been latched.
  // TODO(crbug.com/576351868): Consider exposing the appropriate spec or rules
  // directly instead of `GetProvider`.
  static GlicProvider GetProvider(content::BrowserContext& context);

  // Latches `provider` for `context`. Only `GlicProviderLatchFactory` may call
  // this, once per context.
  static void Set(base::PassKey<GlicProviderLatchFactory>,
                  content::BrowserContext& context,
                  GlicProvider provider);

 private:
  explicit GlicProviderLatch(GlicProvider provider);

  static GlicProviderLatch* Get(content::BrowserContext& context);

  const GlicProvider provider_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_LATCH_H_

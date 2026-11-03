// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_SCOPED_GLIC_PROVIDER_OVERRIDE_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_SCOPED_GLIC_PROVIDER_OVERRIDE_H_

#include <optional>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "chrome/browser/glic/public/glic_provider.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace glic {

// Latches `provider` (see `GlicProviderLatch`) for profiles whose keyed
// services are created while this object is alive, in place of the value that
// `GlicProviderLatchFactory` would resolve. The latch never changes for the
// lifetime of a profile, so this must be created before the targeted profiles
// are, e.g. in a test fixture's constructor or
// `SetUpInProcessBrowserTestFixture()` to cover the initial browser test
// profile, or in the test body right before creating an additional profile.
// Gemini Enterprise is latched with `kTestGeicGuestUrl`, regardless of the GEiC
// configuration.
class ScopedGlicProviderOverride {
 public:
  // Overrides the provider of every profile created while this is alive.
  // Off-the-record and system profiles are also matched, which has no effect
  // since only regular profiles are latched.
  explicit ScopedGlicProviderOverride(GlicProvider provider);
  // Only overrides the provider of the profile created at `profile_path` (and
  // its off-the-record profiles, which aren't latched).
  ScopedGlicProviderOverride(GlicProvider provider,
                             base::FilePath profile_path);
  ScopedGlicProviderOverride(const ScopedGlicProviderOverride&) = delete;
  ScopedGlicProviderOverride& operator=(const ScopedGlicProviderOverride&) =
      delete;
  ~ScopedGlicProviderOverride();

 private:
  ScopedGlicProviderOverride(GlicProvider provider,
                             std::optional<base::FilePath> profile_path);

  void OnCreateServices(content::BrowserContext* context);

  const GlicProvider provider_;
  const std::optional<base::FilePath> profile_path_;
  base::CallbackListSubscription create_services_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_SCOPED_GLIC_PROVIDER_OVERRIDE_H_

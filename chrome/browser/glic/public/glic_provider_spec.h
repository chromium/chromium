// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SPEC_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SPEC_H_

#include <memory>
#include <string_view>

#include "chrome/browser/glic/public/glic_provider.h"
#include "url/gurl.h"

class Profile;

namespace glic {

enum class GlicSettingsSection { kGemini, kGeminiEnterprise };

struct GeicProviderInitOptions {
  GURL guest_url;
};

// Provider-specific data for a profile's Glic surface. Feature code reads
// values from here instead of checking which provider the profile uses.
// Holds data only. Values don't change for a given profile.
class GlicProviderSpec;

// Returns the `GlicProviderSpec` for `profile` owned by `GlicProviderLatch`
// (or a static Consumer spec when `profile` is null or not latched).
const GlicProviderSpec& GetGlicProviderSpec(Profile* profile);

// Creates the per-profile `GlicProviderSpec` stored in `GlicProviderLatch`.
std::unique_ptr<const GlicProviderSpec> CreateGlicProviderSpec(
    GlicProvider provider,
    GeicProviderInitOptions geic_options);

class GlicProviderSpec {
 public:
  GlicProviderSpec() = default;
  GlicProviderSpec(const GlicProviderSpec&) = delete;
  GlicProviderSpec& operator=(const GlicProviderSpec&) = delete;
  virtual ~GlicProviderSpec() = default;

  // The AI settings section to show for this provider.
  virtual GlicSettingsSection GetSettingsSection() const = 0;
  // The settings subpage that OpenGlicSettingsPage opens, e.g. "ai/gemini".
  virtual std::string_view GetSettingsSubpage() const = 0;
  // Resource ID of the entry-point (Glic button) icon.
  virtual int GetEntryPointIconId() const = 0;
  // The synthetic field trial group name for this provider.
  virtual std::string_view GetSyntheticTrialGroupName() const = 0;
  // Infix for per-provider metric names: Glic.<Infix>.X.
  virtual std::string_view GetMetricsInfix() const = 0;
  // Whether browser web actuation (Actor) is supported for this provider.
  virtual bool IsActuationSupported() const = 0;
  // Whether this provider requires NoWebview mode.
  virtual bool RequiresNoWebview() const = 0;
  // Returns the localized guest URL to load for `profile`, or an empty GURL
  // if none is configured.
  virtual GURL GetGuestUrl(Profile* profile) const = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SPEC_H_

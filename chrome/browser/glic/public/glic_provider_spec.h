// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SPEC_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SPEC_H_

#include <memory>
#include <string_view>

#include "chrome/browser/glic/public/glic_feature.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "url/gurl.h"

class Profile;

namespace glic {

enum class GlicSettingsSection { kGemini, kGeminiEnterprise };

struct GeicProviderInitOptions {
  GURL guest_url;
};

class GlicFreStore;

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
  // Whether the entry point stays visible, in an error state, for onboarded
  // profiles that fail a Glic requirement
  // (`features::kGlicAnchorEntryPointForOnboardedUsers`).
  virtual bool SupportsAnchoredEntryPoint() const = 0;
  // The store for the FRE this provider uses for consent.
  // TODO(b/576047045): Bind the FRE store to its profile so its methods no
  // longer need a `Profile*`. Each profile's spec is owned by
  // `GlicProviderLatch`, but `CreateGlicProviderSpec()` doesn't take the
  // profile, and unlatched profiles share a fallback spec.
  virtual const GlicFreStore& GetFreStore() const = 0;
  // Whether enablement applies the Gemini account eligibility checks, i.e.
  // whether the profile's consumer Google account may use Gemini in Chrome:
  // the rollout (including subscription tiers), the primary account's
  // sign-in state and capabilities, the GeminiSettings policy and the
  // server-side user status. This is separate from profile eligibility
  // (GlicEnabling::IsProfileEligible()), which applies to every provider.
  //
  // Gemini Enterprise doesn't apply them: its access is decided elsewhere. A
  // profile is only latched to Gemini Enterprise if the GEiC feature is
  // enabled and a Gemini Enterprise guest URL is configured when the profile
  // is created, normally by the admin through the GeminiEnterpriseUrl policy
  // (see geic::ResolveGeicGuestUrl() and GlicProviderLatchFactory), and the
  // Gemini Enterprise web app handles the user's sign-in and access. The
  // checks shared by all providers (the Glic feature, country, locale,
  // system and OS requirements and FRE consent) still apply.
  virtual bool RequiresGeminiAccountEligibility() const = 0;
  // Whether this provider supports `feature`. This only reports provider
  // support; callers still apply the feature's other requirements.
  virtual bool SupportsFeature(GlicFeature feature) const = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SPEC_H_

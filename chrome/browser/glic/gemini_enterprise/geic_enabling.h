// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_ENABLING_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_ENABLING_H_

#include "chrome/common/chrome_switches.h"
#include "url/gurl.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

// Internal helper for resolving Gemini Enterprise in Chrome (GEiC)
// configuration from Finch, enterprise policy, and the command line.
//
// This library is NOT a general purpose enablement check and must not be
// consulted directly by feature code. It only answers "how is GEiC
// configured?", not "should this profile run in GEiC mode?". The latter is
// owned by GLiC's enablement utilities (see
// GetGlicProviderSpec() / GlicKeyedService::provider_spec() /
// GlicProviderSpec), which layer profile eligibility on top of these helpers.
// The per-profile answer is resolved once, when the profile's keyed services
// are created, and latched by `GlicProviderLatch`. In particular, GEiC is
// unavailable whenever GLiC itself is disabled or ineligible for the profile,
// and that is not reflected here.
//
// Including this header is restricted to an allowlist in
// chrome/browser/glic/DEPS so that callers go through GLiC's enablement
// utilities instead. Reach out to the GEiC owners before adding a new entry to
// that allowlist.
namespace geic {

// Optional command line switch for manual developer overrides of the guest URL.
inline constexpr auto& kGeicGuestURLSwitch = switches::kGeicGuestURL;

// Returns true if the `features::kGeic` feature and its `enabled` parameter are
// enabled, independent of profile state or guest URL configuration.
bool IsGeicEnabledByFeature();

// Returns true if the settings UI may offer switching between Gemini
// Enterprise and consumer Gemini for `browser_context`.
bool CanSwitchBetweenGeminiProvidersInSettings(
    content::BrowserContext* browser_context);

// Resolves the Gemini Enterprise guest URL for `browser_context` from the
// current configuration on every call, without reading or writing the latch, or
// returns an empty GURL() if GEiC is not enabled or no valid URL is configured.
// `GlicProviderLatchFactory` calls this when a profile's keyed services are
// created.
//
// Precedence:
// 1. Command-line switch `--geic-guest-url`.
// 2. Enterprise policy (`GeminiEnterpriseUrl`, stored in
//    `prefs::kGlicGeminiEnterpriseUrl`).
// 3. Finch parameter `features::kGeicGuestURL` (when `features::kGeic` is
//    controlled by a `google_groups`-filtered Finch study, the Finch URL
//    parameter only applies to profiles in those groups).
//
// All sources must be valid HTTPS URLs without embedded credentials, and are
// restricted to Gemini Enterprise hosts in tiers:
// - Policy: production hosts only (`IsAllowedGeminiEnterpriseHost()`).
// - Finch: policy hosts plus pre-production and internal corp hosts
//   (`IsAllowedGeminiEnterprisePreProductionHost()`).
// - `--geic-guest-url`: Finch hosts plus HTTP or HTTPS localhost URLs.
// `--glic-dev` does not relax these checks.
//
// `--geic-guest-url` intentionally takes precedence over the enterprise policy
// to simplify development and local testing on managed profiles. Accessing
// GEiC still requires the user to sign in to Gemini Enterprise.
GURL ResolveGeicGuestUrl(content::BrowserContext* browser_context);

// Returns true if `browser_context` should latch to
// `GlicProvider::kGeminiEnterprise` at profile startup, or false to latch to
// consumer `GlicProvider::kGemini`.
bool ShouldLatchToGeminiEnterprise(content::BrowserContext* browser_context,
                                   const GURL& resolved_guest_url);

// Derives the Gemini Enterprise web app's Skills URL (`/home/cid/<configId>/r/skills`
// or `/r/skills`) from `profile`'s latched guest URL, or returns an empty
// GURL() if `profile` has no valid guest URL.
GURL GetGeicSkillsUrl(Profile* profile);

}  // namespace geic

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_ENABLING_H_

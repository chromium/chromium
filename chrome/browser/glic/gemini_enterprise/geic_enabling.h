// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_ENABLING_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_ENABLING_H_

#include "chrome/common/chrome_switches.h"
#include "url/gurl.h"

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
// GlicKeyedService::provider_spec() / GlicProviderSpec), which layer profile
// eligibility on top of these helpers. The per-profile answer is resolved
// once, when the profile's keyed services are created, and latched by
// `GlicProviderLatch`. In particular, GEiC is unavailable whenever GLiC itself
// is disabled or ineligible for the profile, and that is not reflected here.
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

// Resolves the Gemini Enterprise guest URL for `browser_context` from the
// current configuration on every call, without reading or writing the latch, or
// returns an empty GURL() if GEiC is not enabled or no valid URL is configured.
// `GlicProviderLatchFactory` calls this when a profile's keyed services are
// created.
//
// Precedence:
// 1. Command-line switch `--geic-guest-url`.
// 2. Enterprise policy (`glic.gemini_enterprise_settings.url`).
// 3. Finch parameter `features::kGeicGuestURL` (when `features::kGeic` is
//    controlled by a `google_groups`-filtered Finch study, the Finch URL
//    parameter only applies to profiles in those groups).
GURL ResolveGeicGuestUrl(content::BrowserContext* browser_context);

}  // namespace geic

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_ENABLING_H_

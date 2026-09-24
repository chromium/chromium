// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_ENABLING_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_ENABLING_H_

#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

// Internal helper for resolving Gemini Enterprise in Chrome (GEiC)
// configuration from Finch and the command line.
//
// This library is NOT a general purpose enablement check and must not be
// consulted directly by feature code. It only answers "how is GEiC
// configured?", not "should this profile run in GEiC mode?". The latter is
// owned by GLiC's enablement utilities (see GlicEnabling), which layer GLiC
// eligibility, profile state and enterprise policy on top of these helpers and
// cache the result at profile construction time. In particular, GEiC is
// unavailable whenever GLiC itself is disabled or ineligible, and that is not
// reflected here.
//
// Including this header is restricted to an allowlist in
// chrome/browser/glic/host/DEPS so that callers go through GLiC's enablement
// utilities instead. Reach out to the GEiC owners before adding a new entry to
// that allowlist.
namespace geic {

// Optional command line switch for manual developer overrides of the guest URL.
inline constexpr char kGeicGuestURLSwitch[] = "geic-guest-url";

// Returns true if the `features::kGeic` feature and its `enabled` parameter are
// enabled, independent of profile state or guest URL configuration.
bool IsGeicEnabledByFeature();

// Returns true if Gemini Enterprise in Chrome (GEiC) is enabled for
// `browser_context`.
//
// Requires both `IsGeicEnabledByFeature()` and a valid resolved GEiC guest URL
// for `browser_context`. The result is resolved on the first call and latched
// on `browser_context`, so it does not change for the rest of the session.
bool IsGeicEnabled(content::BrowserContext* browser_context);

// Returns the Gemini Enterprise guest URL for `browser_context` if enabled, or
// empty GURL() if not. Latched together with `IsGeicEnabled()`.
//
// Precedence:
// 1. Command-line switch `--geic-guest-url`.
// 2. Enterprise policy (`glic.gemini_enterprise_settings.url`).
// 3. Finch parameter `features::kGeicGuestURL`.
GURL GetGeicGuestUrl(content::BrowserContext* browser_context);

}  // namespace geic

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_ENABLING_H_

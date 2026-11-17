// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_URL_ALLOWLIST_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_URL_ALLOWLIST_H_

#include <string>
#include <string_view>

#include "base/types/expected.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace geic {

// Query parameter carrying the Workforce Identity provider resource name on
// the unwrapped side-panel URL.
inline constexpr char kWorkforceProviderQueryParam[] = "idp";

// Query parameter added to the popout sign-in `continueUrl` so the GE web
// application knows the tab was opened for side-panel authentication.
inline constexpr char kAuthPopoutQueryParam[] = "auth_popout";

// Query parameter carrying the Cloud Identity session index (`csesidx`).
inline constexpr char kSessionIndexQueryParam[] = "csesidx";

// Returns true if `url`'s host is one of the production hosts that may serve
// the Gemini Enterprise web application. This is the only set accepted for the
// `GeminiEnterpriseUrl` policy. Only the host is checked; callers are
// responsible for scheme and other URL requirements.
bool IsAllowedGeminiEnterpriseHost(const GURL& url);

// Returns true if `url`'s host is a pre-production or internal corp Gemini
// Enterprise host. These hosts are accepted from Finch or the command line.
// Policy accepts only hosts allowed by `IsAllowedGeminiEnterpriseHost()`,
// which may include a host that also matches here (e.g.
// `ucs-widget.corp.google.com`). Hosts are matched by product-prefixed
// patterns (e.g. `business-<label>.gemini.google` or
// `ucs-<label>.corp.google.com`), where `<label>` is a single DNS label. Only
// the host is checked; callers are responsible for scheme and other URL
// requirements.
bool IsAllowedGeminiEnterprisePreProductionHost(const GURL& url);

struct WorkforceAuthPortalInfo {
  GURL inner_url;
  // Empty when `/signin` does not pin a single provider.
  std::string workforce_provider;
};

enum class WorkforceAuthPortalUrlError {
  kNotWorkforceAuthPortal,
  kInvalidUrl,
  kInnerUrlNotHttps,
  kDisallowedInnerHost,
};

// Parses a production Workforce Identity (BYOID) Auth Portal sign-in URL
// (`https://auth.<domain>.google/signin[/<provider>]?continueUrl=<inner>`)
// whose `continueUrl` targets an allowed production Gemini Enterprise host on
// the paired domain. Only a single Auth Portal redirect hop is unwrapped
// (nested wrapper URLs are rejected). Safe to call before `base::FeatureList`
// is initialized.
base::expected<WorkforceAuthPortalInfo, WorkforceAuthPortalUrlError>
ParseWorkforceAuthPortalUrl(const GURL& url);

// Parses a pre-production Workforce Identity Auth Portal sign-in URL
// (`https://auth-<label>.<domain>.google/signin[/<provider>]?continueUrl=<inner>`)
// whose `continueUrl` targets an allowed pre-production Gemini Enterprise host
// on the paired domain. Only a single Auth Portal redirect hop is unwrapped.
// Accepted from Finch or `--geic-guest-url`, not policy.
base::expected<WorkforceAuthPortalInfo, WorkforceAuthPortalUrlError>
ParsePreProductionWorkforceAuthPortalUrl(const GURL& url);

// Returns true if `candidate_origin` is the HTTPS Workforce Identity (BYOID)
// Auth Portal origin paired with `guest_url`'s host.
bool IsPairedWorkforceAuthPortalOrigin(const url::Origin& candidate_origin,
                                       const GURL& guest_url);

// Returns true if `path` on a Workforce Auth Portal host is an interactive
// sign-in endpoint (`/signin*`, `/login*`, `/reauth*`) that cannot silently
// `302` back to the Gemini Enterprise host without interactive authentication.
bool IsWorkforceAuthPortalInteractiveSignInPath(std::string_view path);

// Builds the URL to open in a foreground browser tab when the user clicks
// Sign-In on the side panel overlay.
GURL BuildWorkforceSignInTabUrl(const GURL& guest_url,
                                const GURL& intercepted_url);

}  // namespace geic

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_URL_ALLOWLIST_H_

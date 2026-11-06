// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_URL_ALLOWLIST_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_URL_ALLOWLIST_H_

class GURL;

namespace geic {

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

}  // namespace geic

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_URL_ALLOWLIST_H_

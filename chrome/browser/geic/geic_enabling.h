// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEIC_GEIC_ENABLING_H_
#define CHROME_BROWSER_GEIC_GEIC_ENABLING_H_

class GURL;
class Profile;

namespace geic {

namespace switches {

// Enables GEiC button and feature.
inline constexpr char kGeicEnabled[] = "geic-enabled";

}  // namespace switches

// Returns true if GEiC is enabled for the given `profile`.
bool IsGeicEnabled(Profile* profile = nullptr);

// Validates whether the given URL matches allowed Gemini Enterprise schemes and
// host origins.
bool IsValidGuestUrl(const GURL& url);

// Canonicalizes a Gemini Enterprise URL (e.g., converting a Pantheon console
// URL with path `/home/cid/<configId>` to the `/side-panel?configId=<configId>`
// embed format).
GURL CanonicalizeGuestUrl(const GURL& input_url);

// Returns the validated and canonicalized policy guest URL if configured in
// `profile`'s enterprise settings.
GURL GetPolicyGuestUrl(Profile* profile);

}  // namespace geic

#endif  // CHROME_BROWSER_GEIC_GEIC_ENABLING_H_

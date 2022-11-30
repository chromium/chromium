// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_DEFAULTED_CALLBACKS_H_
#define CHROME_BROWSER_SSL_HTTPS_DEFAULTED_CALLBACKS_H_

namespace content {
class NavigationHandle;
}  // namespace content

// Returns true if a navigation is using HTTPS because it was defaulted to it
// (i.e., schemeless typed omnibox navigations or HTTPS-Only Mode upgrades) and
// it should not trigger SSL error interstitials. These features that upgrade
// navigations to HTTPS have special handling for error cases -- see
// `TypedNavigationUpgradeThrottle` and `HttpsOnlyModeNavigationThrottle`.
bool ShouldIgnoreSslInterstitialBecauseNavigationDefaultedToHttps(
    content::NavigationHandle* handle);

#endif  // CHROME_BROWSER_SSL_HTTPS_DEFAULTED_CALLBACKS_H_

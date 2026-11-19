// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_HELPER_H_
#define CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_HELPER_H_

class BrowserWindowInterface;
class GURL;
class Profile;
class TemplateURLService;

namespace lens {

// Returns whether `profile`'s default search engine resolves to Google
// (`SEARCH_ENGINE_GOOGLE`) for Lens entry-point gating, without enforcing
// `TemplateURL::IsTrustedGoogleEngine()`'s omnibox/suggest endpoint checks.
// Returns false if `profile` is null.
bool IsDefaultSearchProviderGoogleForLens(Profile* profile);

bool IsRegionSearchEnabled(BrowserWindowInterface* browser,
                           Profile* profile,
                           TemplateURLService* service,
                           const GURL& url);

bool IsInProgressiveWebApp(BrowserWindowInterface* browser);

}  // namespace lens

#endif  // CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_HELPER_H_

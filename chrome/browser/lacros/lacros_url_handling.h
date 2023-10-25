// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_URL_HANDLING_H_
#define CHROME_BROWSER_LACROS_LACROS_URL_HANDLING_H_

class GURL;
struct NavigateParams;

namespace lacros_url_handling {

// Checks if the given navigation is allowable to intercept a navigation and
// redirects it to Ash. This function does not test the target URL, but the
// navigation |params| and the |source_url| (the page which is asking for the
// navigation).
bool IsNavigationInterceptable(const NavigateParams& params,
                               const GURL& source_url);

// Provides an opportunity for the URL to be intercepted and handled by Ash.
// This is used for example to handle system chrome:// URLs that only Ash knows
// how to load. Returns |true| if the navigation was intercepted.
bool MaybeInterceptNavigation(const GURL& url);

// This is an explicit url redirect executed in Ash. It returns |true| when
// a navigation has been forwarded to Ash. This call is used for chrome:// and
// os:// URLs which are handled either by Ash or Ash and Lacros.
// Note that only Ash allow listed url's can be called.
bool NavigateInAsh(GURL url);

// Exposed for testing only.
// Checks if Ash is allowed and able to handle the requested URL or not.
bool IsUrlAcceptedByAsh(const GURL& url);

// Exposed for testing only.
// Checks if Lacros is able to handle the requested URL or not.
bool IsUrlHandledByLacros(const GURL& url);

}  // namespace lacros_url_handling

#endif  // CHROME_BROWSER_LACROS_LACROS_URL_HANDLING_H_

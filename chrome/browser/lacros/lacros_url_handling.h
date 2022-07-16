// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_URL_HANDLING_H_
#define CHROME_BROWSER_LACROS_LACROS_URL_HANDLING_H_

class GURL;

namespace lacros_url_handling {

// Provides an opportunity for the URL to be intercepted and handled by Ash.
// This is used for example to handle system chrome:// URLs that only Ash knows
// how to load. Returns |true| if the navigation was intercepted.
bool MaybeInterceptNavigation(const GURL& url);

// This is an explicit url redirect executed in Ash. It returns |true| when
// a navigation has been forwarded to Ash. This call is used for chrome:// and
// os:// URLs which are handled either by Ash or Ash and Lacros.
// Note that only Ash allow listed url's can be called.
bool NavigateInAsh(const GURL& url);

}  // namespace lacros_url_handling

#endif  // CHROME_BROWSER_LACROS_LACROS_URL_HANDLING_H_

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_COOKIE_ACCESS_FILTER_H_
#define CHROME_BROWSER_DIPS_COOKIE_ACCESS_FILTER_H_

#include <vector>

#include "chrome/browser/dips/dips_utils.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "url/gurl.h"

// Filters a chain of URLs to the ones which accessed cookies.
//
// Intended for use by a WebContentsObserver which overrides OnCookiesAccessed
// and DidFinishNavigation.
class CookieAccessFilter {
 public:
  CookieAccessFilter();
  ~CookieAccessFilter();

  using Type = network::mojom::CookieAccessDetails::Type;

  // Record that `url` accessed cookies.
  void AddAccess(const GURL& url, Type type);

  // Clear `result` and fill it with the the type of cookie access for each URL.
  // `result` will have the same length as `urls`. Returns true iff every
  // previously-recorded cookie access was successfully matched to a URL in
  // `urls`. (Note: this depends on the order of previous calls to AddAccess()).
  [[nodiscard]] bool Filter(const std::vector<GURL>& urls,
                            std::vector<CookieAccessType>* result) const;

  // Returns true iff AddAccess() has never been called.
  bool is_empty() const { return accesses_.empty(); }

 private:
  struct CookieAccess {
    GURL url;
    CookieAccessType type = CookieAccessType::kNone;
  };

  // We use a vector rather than a set of URLs because order can matter. If the
  // same URL appears twice in a redirect chain, we might be able to distinguish
  // between them.
  std::vector<CookieAccess> accesses_;
};

#endif  // CHROME_BROWSER_DIPS_COOKIE_ACCESS_FILTER_H_

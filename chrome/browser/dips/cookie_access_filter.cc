// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/cookie_access_filter.h"

#include "chrome/browser/dips/dips_utils.h"

CookieAccessFilter::CookieAccessFilter() = default;
CookieAccessFilter::~CookieAccessFilter() = default;

void CookieAccessFilter::AddAccess(const GURL& url, CookieOperation op) {
  SiteDataAccessType t = ToSiteDataAccessType(op);
  if (!accesses_.empty() && accesses_.back().url == url) {
    // Coalesce accesses for the same URL. They may have come from separate
    // visits, but we can't distinguish them from redundant calls, which are
    // more likely.
    accesses_.back().type = accesses_.back().type | t;
    return;
  }
  accesses_.push_back({url, t});
}

// This method attempts to match every entry of this->accesses_ with a member of
// `urls`, in order. Other URLs will be treated as kNone.
//
// Imagine `urls` contains 5 unique URLs [A, B, C, D, E] and this->accesses_
// contains [(B, kRead), (D, kWrite)]; then this will store in `result`: [kNone,
// kRead, kNone, kWrite, kNone].
//
// It's complicated by the fact that AddAccess() can be called multiple times
// redundantly for a single URL visit, so it must coalesce them (see
// crbug.com/1335510); yet it's theoretically possible for one URL to be visited
// multiple times, even consecutively, in a single redirect chain.
//
// To handle that corner case (imperfectly), if the same URL appears multiple
// times in a row, it will get the same SiteDataAccessType for all of them.
bool CookieAccessFilter::Filter(const std::vector<GURL>& urls,
                                std::vector<SiteDataAccessType>* result) const {
  result->clear();
  result->resize(urls.size(), SiteDataAccessType::kNone);

  size_t url_idx = 0;
  size_t access_idx = 0;
  // `matched` is true when accesses_[access_idx] has already matched a URL in
  // `urls`.
  bool matched = false;
  while (access_idx < accesses_.size() && url_idx < urls.size()) {
    if (urls[url_idx] == accesses_[access_idx].url) {
      // Cookie URL matches redirect URL. Copy the access type to `result`.
      //
      // Move on to the next redirect URL, but keep trying the same cookie URL
      // (in case we coalesced multiple visits into a single accesses_ entry).
      (*result)[url_idx] = accesses_[access_idx].type;
      ++url_idx;
      matched = true;
    } else if (matched) {
      // Cookie URL doesn't match redirect URL, but the current cookie URL
      // matched the previous URL in the redirect chain.
      //
      // Move onto the next cookie URL, and try the same redirect URL again.
      ++access_idx;
      matched = false;
    } else {  // !matched
      // Cookie URL doesn't match redirect URL, nor did it match the previous
      // URL in the redirect chain.
      //
      // We need to find a match for this cookie URL, so move onto the next
      // redirect URL, and keep trying the same cookie URL.
      //
      // Note: `result` was prefilled with kNone, so we don't have to modify it
      // here.
      ++url_idx;
    }
  }

  // Return true iff we consumed all the cookie accesses recorded by calls to
  // AddAccess().
  if (access_idx == accesses_.size() ||
      (access_idx == accesses_.size() - 1 && matched)) {
    return true;
  }

  // Otherwise, fill the entire result vector with kUnknown and return false.
  std::fill(result->begin(), result->end(), SiteDataAccessType::kUnknown);
  return false;
}

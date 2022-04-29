// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/cookie_access_filter.h"

#include "base/strings/string_piece.h"

// CookieAccessFilter depends on two important assumptions:
// 1. If a redirect both reads and writes cookies, then AddAccess() will be
//    called with kRead before kChange. This is the order that
//    WebContentsObserver::OnCookiesAccessed() is called with. It is logical
//    because the cookies are first read to attach to the HTTP request, and
//    later cookies are written in accordance with the response received.
// 2. Within a single redirect chain, if a URL is visited twice, then if cookies
//    are read on the first visit, cookies will also be read on the second
//    visit, if there are no writes in between. This is logical because the
//    browser always sends all relevant cookies with each HTTP request. Any
//    cookies relevant to the first visit would also apply to the second (unless
//    they expired or were cleared at that precise moment -- extremely unlikely,
//    since this is within a redirect chain).
//
// These two assumptions lead to an important corollary: a read followed by a
// write for the same URL (with no other URLs in between) must have come from a
// single redirect in the chain. Also, there is no other situation where two
// calls to AddAccess() represent access by a single redirect.
//
// The first assumption is checked by a test in
// dips_bounce_detector_browsertest.cc. If the assumption is not true, the test
// will be flaky.

CookieAccessFilter::CookieAccessFilter() = default;
CookieAccessFilter::~CookieAccessFilter() = default;

void CookieAccessFilter::AddAccess(const GURL& url, Type type) {
  if (type == Type::kChange && !accesses_.empty() &&
      accesses_.back().type == CookieAccessType::kRead &&
      accesses_.back().url == url) {
    // If this access is a write for the same url that was just read, coalesce
    // them.
    accesses_.back().type = CookieAccessType::kReadWrite;
    return;
  }
  accesses_.push_back({url, type == Type::kChange ? CookieAccessType::kWrite
                                                  : CookieAccessType::kRead});
}

bool CookieAccessFilter::Filter(const std::vector<GURL>& urls,
                                std::vector<CookieAccessType>* result) const {
  result->clear();
  result->resize(urls.size(), CookieAccessType::kNone);

  size_t access_idx = 0;
  for (size_t url_idx = 0;
       access_idx < accesses_.size() && url_idx < urls.size(); url_idx++) {
    if (urls[url_idx] == accesses_[access_idx].url) {
      (*result)[url_idx] = accesses_[access_idx].type;
      ++access_idx;
    }
  }

  // Return true iff we consumed all the cookie accesses recorded by calls to
  // AddAccess().
  return access_idx == accesses_.size();
}

base::StringPiece CookieAccessTypeToString(CookieAccessType type) {
  switch (type) {
    case CookieAccessType::kNone:
      return "None";
    case CookieAccessType::kRead:
      return "Read";
    case CookieAccessType::kWrite:
      return "Write";
    case CookieAccessType::kReadWrite:
      return "ReadWrite";
  }
}

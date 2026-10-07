// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_UTIL_H_
#define CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_UTIL_H_

class GURL;

namespace ttc {

// Returns whether TTC may fetch and surface page context for a page at `url`.
// Only the New Tab Page and HTTP(S) pages are supported.
bool IsUrlSupportedForPageContext(const GURL& url);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_UTIL_H_

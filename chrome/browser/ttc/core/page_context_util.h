// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_UTIL_H_
#define CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_UTIL_H_

#include "base/functional/callback_forward.h"

class GURL;

namespace ttc {

// Determines whether TTC may surface page context for a page at `url`, and
// invokes `callback` with the result.
//
// Only the New Tab Page and HTTP(S) pages are supported, and the URL must not
// be blocked by the actor safety lists (actor::SafetyListManager), evaluated
// as a navigation from `url` to itself. The safety list check fails open: if
// the lists have not been loaded (or failed to parse), the URL is not blocked
// by them.
void IsUrlSupportedForPageContext(const GURL& url,
                                  base::OnceCallback<void(bool)> callback);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_UTIL_H_

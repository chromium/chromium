// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_UTIL_H_
#define CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_UTIL_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/ttc/app/public/page_context_types.h"
#include "components/ttc/app/public/tool_types.h"

class GURL;

namespace actor {
class EnterprisePolicyChecker;
}

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace ttc {

// Returns the synchronous `PageRestrictionReason` when `url` has an
// unsupported scheme or host (only the New Tab Page and HTTP(S) pages are
// supported), or when `enterprise_policy_checker` explicitly blocks `url`.
// Returns `std::nullopt` when asynchronous checks are needed.
std::optional<PageRestrictionReason> GetPageRestrictionForUrlSync(
    const GURL& url,
    const actor::EnterprisePolicyChecker* enterprise_policy_checker);

// Evaluates whether TTC must withhold page context for a page at `url`, and
// invokes `callback` with the `PageRestrictionReason`, or `std::nullopt` when
// page context may be shared.
//
// Unlike `GetPageRestrictionForUrlSync()`, this also checks the actor safety
// lists (`actor::SafetyListManager`) asynchronously, evaluated as a navigation
// from `url` to itself. The safety list check fails open: if the lists have
// not been loaded (or failed to parse), the URL is not blocked by them.
void GetPageRestrictionForUrl(
    const GURL& url,
    const actor::EnterprisePolicyChecker* enterprise_policy_checker,
    base::OnceCallback<void(std::optional<PageRestrictionReason>)> callback);

// For the tools that capture the page, once its URL has been checked (see
// GetPageRestrictionForUrl()): returns the error to answer with if the URL
// is not supported, if `page`, whose URL it was, is no longer the primary page
// of `focused_contents`, the contents the session is focused on now, which may
// be null, or if its renderer has crashed. Returns nullopt if `page` may be
// captured.
//
// TODO(crbug.com/571268337): Centralize this. What TTC requests page context
// through (APC, text, screenshots) should also say whether the current page
// may be captured, e.g. by an IsCurrentPageEligible() method that a tool asks
// before it captures.
std::optional<ToolResponse> CheckPageForCapture(
    content::WebContents* focused_contents,
    const base::WeakPtr<content::Page>& page,
    bool is_url_supported);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_UTIL_H_

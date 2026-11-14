// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_UTIL_H_
#define CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_UTIL_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/actor/public/mojom/actor_types.mojom-forward.h"
#include "components/ttc/app/public/tool_types.h"

class GURL;

namespace actor {
class EnterprisePolicyChecker;
}

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace page_content_annotations {
enum class FetchPageContextError;
}

namespace ttc {

// Determines whether TTC may surface page context for a page at `url`, and
// invokes `callback` with the result.
//
// Only the New Tab Page and HTTP(S) pages are supported. If
// `enterprise_policy_checker` is provided, the URL must not be explicitly
// blocked by enterprise policy. Additionally, the URL must not be blocked by
// the actor safety lists (actor::SafetyListManager), evaluated as a
// navigation from `url` to itself. The safety list check fails open: if the
// lists have not been loaded (or failed to parse), the URL is not blocked by
// them.
void IsUrlSupportedForPageContext(
    const GURL& url,
    const actor::EnterprisePolicyChecker* enterprise_policy_checker,
    base::OnceCallback<void(bool)> callback);

// For the tools that capture the page, once its URL has been checked (see
// IsUrlSupportedForPageContext()): returns the error to answer with if the URL
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

// Returns the tool error for a screenshot whose page context fetch failed
// with `error`.
actor::mojom::ActionResultCode ToScreenshotErrorCode(
    page_content_annotations::FetchPageContextError error);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_UTIL_H_

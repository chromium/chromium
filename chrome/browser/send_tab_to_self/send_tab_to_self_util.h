// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

#include <optional>

#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/send_tab_to_self/page_context.h"

namespace content {
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace send_tab_to_self {

// `web_contents` can be null.
std::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
    content::WebContents* web_contents);

// Returns true if the entry point should be shown.
bool ShouldDisplayEntryPoint(content::WebContents* web_contents);

// Creates a PageContext for the given `web_contents` by extracting form data
// from all frames.
PageContext ExtractFormFieldsFromWebContents(
    content::WebContents* web_contents);

// Fills form fields in `web_contents` from `page_context` if the field's origin
// matches `origin`.
void FillWebContents(content::WebContents* web_contents,
                     const url::Origin& origin,
                     const PageContext& page_context);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_UTIL_H_

#include <string>

#include "base/types/expected.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class BrowserWindowInterface;
class ExtensionFunction;
class Profile;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {

class WindowController;

// Helpers shared by the chrome.tabs and chrome.windows API implementations.
namespace tabs_util {

// Retrieves the tab associated with the given `tab_id`, populating
// `contents_out`, `window_out`, and `index_out` with the result. If the tab
// isn't found and `error_out` is non-null, populates `error_out` with an
// appropriate error.
// Returns true if the tab was found.
bool GetTabById(int tab_id,
                content::BrowserContext* context,
                bool include_incognito,
                WindowController** window_out,
                content::WebContents** contents_out,
                int* index_out,
                std::string* error_out);

// Ensures that the tab (`web_contents` in `source_window`) can be moved to a
// window belonging to `window_profile`.
base::expected<void, std::string> ValidateTabForMove(
    WindowController* source_window,
    Profile* window_profile,
    content::WebContents* web_contents,
    bool is_locked_fullscreen = false);

// Moves the given tab to the `target_browser`. On success, returns the new
// index of the tab in the target tabstrip. On failure, returns -1. Assumes that
// the caller has already checked whether the target window is different from
// the source. `allow_other_window_types` indicates whether moving tabs to
// windows with types other than BrowserWindowInterface::TYPE_NORMAL is
// supported; this is allowed in certain cases (like moving a tab to a popup).
int MoveTabToWindow(ExtensionFunction* function,
                    int tab_id,
                    BrowserWindowInterface* target_browser,
                    int new_index,
                    bool allow_other_window_types,
                    std::string* error);

}  // namespace tabs_util

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_UTIL_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_OPEN_TAB_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_OPEN_TAB_HELPER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"

class ExtensionFunction;
class GURL;

namespace content {
class WebContents;
}

namespace extensions {

class OpenTabHelper {
 public:
  struct Params {
    Params();
    ~Params();

    bool create_browser_if_needed = false;
    std::optional<int> window_id;
    std::optional<bool> active;
    std::optional<bool> pinned;
    std::optional<int> index;
    std::optional<int> bookmark_id;

    raw_ptr<content::WebContents> opener_tab = nullptr;
  };

  // Opens a new tab given an extension function `function` and creation
  // parameters `params`. If a tab can be produced, it will return the newly-
  // added WebContents for the tab; otherwise, it will optionally return an
  // error message, if any is appropriate.
  // `validated_url` must be validated prior to calling this; use
  // ExtensionTabUtil::PrepareURLForNavigation().
  static base::expected<content::WebContents*, std::string> OpenTab(
      const GURL& validated_url,
      ExtensionFunction* function,
      const Params& params,
      bool user_gesture);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_OPEN_TAB_HELPER_H_

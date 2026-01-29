// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_OPEN_TAB_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_OPEN_TAB_HELPER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"

class BrowserWindowInterface;
class ExtensionFunction;
class GURL;
struct NavigateParams;

namespace content {
class WebContents;
}

namespace extensions {

class OpenTabHelper {
 public:
  struct Params {
    Params();
    ~Params();

    std::optional<bool> active;
    std::optional<bool> pinned;
    std::optional<int> index;
    std::optional<int> bookmark_id;
  };

#if !BUILDFLAG(IS_ANDROID)
  // Finds the current browser or creates a new browser that's appropriate to
  // show the given `validated_url`. Returns an error on failure.
  static base::expected<BrowserWindowInterface*, std::string>
  FindOrCreateBrowser(const GURL& validated_url,
                      ExtensionFunction& function,
                      bool create_if_needed);
#endif

  // Opens a new tab given an extension function `function` and creation
  // parameters `params`. If a tab can be produced, it will return the newly-
  // added WebContents for the tab; otherwise, it will optionally return an
  // error message, if any is appropriate.
  // `validated_url` must be validated prior to calling this; use
  // ExtensionTabUtil::PrepareURLForNavigation().
  static base::expected<content::WebContents*, std::string> OpenTab(
      const GURL& validated_url,
      BrowserWindowInterface& browser,
      const ExtensionFunction& function,
      const Params& params);

  // If `function` is for the PDF Viewer, then mark the PDF-initiated navigation
  // as renderer-initiated in `navigate_params` and return true. Otherwise
  // return false and `navigate_params` remains the same.
  static bool MaybeSetPdfNavigateParams(const ExtensionFunction& function,
                                        NavigateParams& navigate_params);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_OPEN_TAB_HELPER_H_

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_DETAILS_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_DETAILS_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/native_widget_types.h"

class ExtensionFunction;

namespace extensions {
class WindowController;
}

// Provides Chrome-specific details to ExtensionFunction
// implementations.
class ChromeExtensionFunctionDetails {
 public:
  // Constructs a new ChromeExtensionFunctionDetails instance for |function|.
  // This instance does not own |function|. |function| must outlive this
  // instance.
  explicit ChromeExtensionFunctionDetails(ExtensionFunction* function);

  ChromeExtensionFunctionDetails(const ChromeExtensionFunctionDetails&) =
      delete;
  ChromeExtensionFunctionDetails& operator=(
      const ChromeExtensionFunctionDetails&) = delete;

  ~ChromeExtensionFunctionDetails();

  // Gets the "current" WindowController, if any.
  //
  // Many extension APIs operate relative to the current window, which is the
  // browser window the calling code is running inside of. For example, popups
  // and tabs all have a containing browser, but background pages and
  // notification bubbles do not.
  //
  // If there is no containing window, the current browser defaults to the
  // foremost one.
  //
  // Incognito browsers are not considered unless the calling extension has
  // incognito access enabled.
  //
  // This method can return NULL if there is no matching browser window, which
  // can happen if only incognito windows are open, or early in startup or
  // shutdown shutdown when there are no active windows.
  //
  // TODO(devlin): This method is incredibly non-deterministic (sometimes just
  // returning "any" browser), and almost never the right thing to use. Instead,
  // use ExtensionFunction::GetSenderWebContents(). We should get rid of this.
  extensions::WindowController* GetCurrentWindowController() const;

  // Find a UI surface to display any UI (like a permission prompt) for the
  // extension calling this function. This will check, in order of preference,
  // - The current window for the function (as defined by
  //   WindowControllerList::CurrentWindowForFunction()),
  // - The sender web contents
  // - Open app windows
  // - A browser with the same profile
  gfx::NativeWindow GetNativeWindowForUI();

  // Returns a pointer to the associated ExtensionFunction
  ExtensionFunction* function() { return function_; }
  const ExtensionFunction* function() const { return function_; }

 private:
  // The function for which these details have been created. Must outlive the
  // ChromeExtensionFunctionDetails instance.
  raw_ptr<ExtensionFunction> function_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_DETAILS_H_

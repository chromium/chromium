// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_UTIL_H__
#define CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_UTIL_H__

#include <string>
#include <vector>

#include "chrome/browser/extensions/window_controller.h"

class ExtensionFunction;
class Profile;
class GURL;

namespace windows_util {

// Populates `*controller` for given `window_id`. If the window is not found,
// returns false and sets `error`.
bool GetControllerFromWindowID(ExtensionFunction* function,
                               int window_id,
                               extensions::WindowController::TypeFilter filter,
                               extensions::WindowController** controller,
                               std::string* error);

// Returns true if `function` (and the profile and extension that it was
// invoked from) can operate on the window wrapped by `window_controller`.
// If `all_window_types` is set this function will return true for any
// kind of window (including app and devtools), otherwise it will
// return true only for normal browser windows as well as windows
// created by the extension.
bool CanOperateOnWindow(const ExtensionFunction* function,
                        const extensions::WindowController* controller,
                        extensions::WindowController::TypeFilter filter);

// Returns true if `function` was called from a logical child window of the
// window wrapped by `controller`.
bool CalledFromChildWindow(ExtensionFunction* function,
                           const extensions::WindowController* controller);

// Enum return value for `ShouldOpenIncognitoWindow`, indicating whether to use
// incognito or the presence of an error.
enum IncognitoResult { kRegular, kIncognito, kError };

// Returns whether the window should be created in incognito mode. `incognito`
// is the optional caller preference. `urls` is the list of urls to open. If
// we are creating an incognito window, the function will remove these urls
// which may not be opened in incognito mode.  If window creation leads the
// browser into an erroneous state, `error` is populated.
IncognitoResult ShouldOpenIncognitoWindow(Profile* profile,
                                          std::optional<bool> incognito,
                                          std::vector<GURL>* urls,
                                          std::string* error);

}  // namespace windows_util

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_UTIL_H__

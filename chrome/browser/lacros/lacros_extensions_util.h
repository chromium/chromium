// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_EXTENSIONS_UTIL_H_
#define CHROME_BROWSER_LACROS_LACROS_EXTENSIONS_UTIL_H_

#include <string>

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
}  // namespace extensions

// Utility functions for lacros extensions or extension apps.

namespace lacros_extensions_util {

// Returns true if |extension| is an extension based app supported in Lacros,
// which include platform apps and hosted apps.
bool IsExtensionApp(const extensions::Extension* extension);

// Returns the extension pointer for |extension_id| in |profile|, or null if
// nonexistent.
const extensions::Extension* MaybeGetExtension(Profile* profile,
                                               const std::string& extension_id);

// Returns the extension pointer for |web_contents|, or null if nonexistent.
const extensions::Extension* MaybeGetExtension(
    content::WebContents* web_contents);

// Gets the profile and extension from |extension_id|. On success, returns true
// and populates variables |output_profile| and |output_extension|. We pass a
// Profile** and an Extension** instead of Profile*& and Extension*& for clarity
// -- the root problem is that Profiles and Extensions are always passed by raw
// pointer to begin with.
bool GetProfileAndExtension(const std::string& extension_id,
                            Profile** output_profile,
                            const extensions::Extension** output_extension);

}  // namespace lacros_extensions_util

#endif  // CHROME_BROWSER_LACROS_LACROS_EXTENSIONS_UTIL_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_EXTENSIONS_UTIL_H_
#define CHROME_BROWSER_LACROS_LACROS_EXTENSIONS_UTIL_H_

#include <string>

class Profile;

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

// Returns a muxed id that consists of the profile base name joined to the
// extension id.
std::string MuxId(const Profile* profile,
                  const extensions::Extension* extension);

// Takes |muxed_id| and extracts the corresponding Profile* and Extension*. On
// success, returns true and populates variables |output_profile| and
// |output_extension|. We pass a Profile** and an Extension** instead of
// Profile*& and Extension*& for clarity -- the root problem is that Profiles
// and Extensions are always passed by raw pointer to begin with.
bool DemuxId(const std::string& muxed_id,
             Profile** output_profile,
             const extensions::Extension** output_extension);

// Same as DemuxId(), but requires the Extension* to be a v2 platform app.
bool DemuxPlatformAppId(const std::string& muxed_id,
                        Profile** output_profile,
                        const extensions::Extension** output_extension);

}  // namespace lacros_extensions_util

#endif  // CHROME_BROWSER_LACROS_LACROS_EXTENSIONS_UTIL_H_

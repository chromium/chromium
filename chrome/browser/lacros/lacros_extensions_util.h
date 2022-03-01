// Copyright 2021 The Chromium Authors. All rights reserved.
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

// Returns the extension pointer for |extension_id| in |profile|, or null if
// nonexistent.
const extensions::Extension* MaybeGetExtension(Profile* profile,
                                               const std::string& extension_id);

// Returns a muxed id that consists of the profile base name joined to the
// extension id.
std::string MuxId(const Profile* profile,
                  const extensions::Extension* extension);
std::string MuxId(const Profile* profile, const std::string& extension_id);

// Takes |muxed_id| and extracts the corresponding Profile* and Extension*,
// while requiring the Extension* to be a v2 platform app. On success, returns
// true and populates the output variables |output_profile| and
// |output_extension|. We pass a Profile** and an Extension** rather than
// Profile*& and Extension*& for clarity -- the root problem is that Profiles
// and Extensions are always passed by raw pointer to begin with.
bool DemuxPlatformAppId(const std::string& muxed_id,
                        Profile** output_profile,
                        const extensions::Extension** output_extension);

}  // namespace lacros_extensions_util

#endif  // CHROME_BROWSER_LACROS_LACROS_EXTENSIONS_UTIL_H_

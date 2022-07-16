// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_UTILITY_H_
#define CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_UTILITY_H_

#include <string>

namespace extensions {
class Extension;
}  // namespace extensions

class Profile;

// This file contains utility functions shared by lacros extension app-related
// classes.
namespace lacros_extension_apps_utility {
// Returns a muxed id that consists of the profile base name joined to the
// extension id.
std::string MuxId(const Profile* profile,
                  const extensions::Extension* extension);

// Returns true on success, and populates the output variables |profile| and
// |extension|. We pass a Profile** and an Extension** rather than Profile*& and
// Extension*& for clarity -- the root problem is that Profiles and Extensions
// are always passed by raw pointer to begin with.
bool DemuxId(const std::string& muxed_id,
             Profile** profile,
             const extensions::Extension** extension);

// Returns an extension pointer if |app_id| corresponds to a packaged v2 app.
const extensions::Extension* MaybeGetPackagedV2App(Profile* profile,
                                                   const std::string& app_id);
}  // namespace lacros_extension_apps_utility

#endif  // CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_UTILITY_H_

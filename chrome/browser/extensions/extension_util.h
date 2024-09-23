// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "extensions/common/constants.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class PermissionSet;
}

class Profile;

namespace extensions {

class Extension;

namespace util {

// Returns true if the extension associated with |extension_id| has isolated
// storage. This can be either because it is an app that requested this in its
// manifest, or because it is a policy-installed app or extension running on
// the Chrome OS sign-in profile.
bool HasIsolatedStorage(const std::string& extension_id,
                        content::BrowserContext* context);
bool HasIsolatedStorage(const Extension& extension,
                        content::BrowserContext* context);

// Sets whether |extension_id| can run in an incognito window. Reloads the
// extension if it's enabled since this permission is applied at loading time
// only. Note that an ExtensionService must exist.
void SetIsIncognitoEnabled(const std::string& extension_id,
                           content::BrowserContext* context,
                           bool enabled);

// Sets whether |extension_id| can inject scripts into pages with file URLs.
// Reloads the extension if it's enabled since this permission is applied at
// loading time only. Note than an ExtensionService must exist.
void SetAllowFileAccess(const std::string& extension_id,
                        content::BrowserContext* context,
                        bool allow);

// Returns true if |extension| should be synced.
bool ShouldSync(const Extension* extension, content::BrowserContext* context);

// Returns true if |extension_id| is idle and it is safe to perform actions such
// as updating.
bool IsExtensionIdle(const std::string& extension_id,
                     content::BrowserContext* context);

// Sets the name, id, and icon resource path of the given extension into the
// returned dictionary.
base::Value::Dict GetExtensionInfo(const Extension* extension);

// Returns a PermissionSet configured with the permissions that should be
// displayed in an extension installation prompt for the specified |extension|.
std::unique_ptr<const PermissionSet> GetInstallPromptPermissionSetForExtension(
    const Extension* extension,
    Profile* profile);

// Returns all profiles affected by permissions of an extension running in
// "spanning" (rather than "split) mode.
std::vector<content::BrowserContext*> GetAllRelatedProfiles(
    Profile* profile,
    const Extension& extension);

// Sets whether the given `profile` is in developer mode and notifies
// relevant subsystems.
void SetDeveloperModeForProfile(Profile* profile, bool in_developer_mode);

// Returns the extension name to be used in UI surfaces. Name will be truncated
// if its very long, preventing extension name to spoof or break UI surfaces
// (see crbug.com/40063885).
std::u16string GetFixupExtensionNameForUIDisplay(
    const std::string& extension_name);

}  // namespace util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_

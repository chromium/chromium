// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "components/favicon_base/favicon_callback.h"
#include "content/public/browser/web_ui_controller.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// A collection of methods to handle Chrome URL overrides that are managed by
// extensions (such as overriding the new tab page).
// TODO(devlin): Rename this class to ExtensionURLOverrides.
class ExtensionWebUI {
 public:
  static const char kExtensionURLOverrides[];

  static bool HandleChromeURLOverride(GURL* url,
                                      content::BrowserContext* browser_context);
  static bool HandleChromeURLOverrideReverse(
      GURL* url, content::BrowserContext* browser_context);

  // Initialize the Chrome URL overrides. This must happen *before* any further
  // calls for URL overrides!
  static void InitializeChromeURLOverrides(Profile* profile);

  // Validate the Chrome URL overrides, ensuring that each is valid and points
  // to an existing extension. To be called once all extensions are loaded.
  static void ValidateChromeURLOverrides(Profile* profile);

  // Add new Chrome URL overrides. If an entry exists, it is marked as active.
  // If one doesn't exist, it is added at the beginning of the list of
  // overrides (meaning it has priority).
  static void RegisterOrActivateChromeURLOverrides(
      Profile* profile,
      const extensions::URLOverrides::URLOverrideMap& overrides);

  // Deactivate overrides without removing them from the list or modifying their
  // positions in the list.
  static void DeactivateChromeURLOverrides(
      Profile* profile,
      const extensions::URLOverrides::URLOverrideMap& overrides);

  // Completely unregister overrides, removing them from the list.
  static void UnregisterChromeURLOverrides(
      Profile* profile,
      const extensions::URLOverrides::URLOverrideMap& overrides);

  // Called from BrowserPrefs
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Get the favicon for the extension by getting an icon from the manifest.
  // Note. |callback| is always run asynchronously.
  static void GetFaviconForURL(Profile* profile,
                               const GURL& page_url,
                               favicon_base::FaviconResultsCallback callback);

 private:
  // Unregister the specified override, and if it's the currently active one,
  // ensure that something takes its place.
  static void UnregisterAndReplaceOverride(const std::string& page,
                                           Profile* profile,
                                           base::ListValue* list,
                                           const base::Value* override);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_H_

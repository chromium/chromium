// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSION_TAB_UTIL_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSION_TAB_UTIL_DELEGATE_CHROMEOS_H_

#include "base/macros.h"
#include "chrome/browser/extensions/extension_tab_util.h"

namespace extensions {
class Extension;

// In Public Sessions, apps and extensions are force-installed by admin policy
// so the user does not get a chance to review the permissions for these apps.
// This is not acceptable from a security standpoint, so we scrub the URL
// returned by chrome.tabs API down to the origin unless the extension ID is
// whitelisted.
class ExtensionTabUtilDelegateChromeOS : public ExtensionTabUtil::Delegate {
 public:
  ExtensionTabUtilDelegateChromeOS();
  ~ExtensionTabUtilDelegateChromeOS() override;

  // ExtensionTabUtil::Delegate
  ExtensionTabUtil::ScrubTabBehaviorType GetScrubTabBehavior(
      const Extension* extension) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionTabUtilDelegateChromeOS);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSION_TAB_UTIL_DELEGATE_CHROMEOS_H_

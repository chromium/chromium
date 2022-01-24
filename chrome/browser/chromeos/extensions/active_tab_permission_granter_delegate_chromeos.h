// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_ACTIVE_TAB_PERMISSION_GRANTER_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_ACTIVE_TAB_PERMISSION_GRANTER_DELEGATE_CHROMEOS_H_

#include "chrome/browser/chromeos/extensions/public_session_permission_helper.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"

namespace content {
class WebContents;
}

namespace extensions {
class Extension;

// In Public Sessions, apps and extensions are force-installed by admin policy
// so the user does not get a chance to review the permissions for these apps.
// This is not acceptable from a security standpoint, so we show a permission
// prompt the first time an extension tries to use activeTab permission (unless
// the extension is whitelisted).
class ActiveTabPermissionGranterDelegateChromeOS
    : public ActiveTabPermissionGranter::Delegate {
 public:
  ActiveTabPermissionGranterDelegateChromeOS();

  ActiveTabPermissionGranterDelegateChromeOS(
      const ActiveTabPermissionGranterDelegateChromeOS&) = delete;
  ActiveTabPermissionGranterDelegateChromeOS& operator=(
      const ActiveTabPermissionGranterDelegateChromeOS&) = delete;

  ~ActiveTabPermissionGranterDelegateChromeOS() override;

  static void SetRequestResolvedCallbackForTesting(
      permission_helper::RequestResolvedCallback* callback);

  // ActiveTabPermissionGranter::Delegate
  bool ShouldGrantActiveTabOrPrompt(
      const Extension* extension,
      content::WebContents* web_contents) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_ACTIVE_TAB_PERMISSION_GRANTER_DELEGATE_CHROMEOS_H_

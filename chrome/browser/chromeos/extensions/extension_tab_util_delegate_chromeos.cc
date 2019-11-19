// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/extension_tab_util_delegate_chromeos.h"

#include <memory>
#include <string>

#include "chrome/browser/chromeos/extensions/device_local_account_management_policy_provider.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/extensions/api/tabs.h"
#include "url/gurl.h"

namespace extensions {

ExtensionTabUtilDelegateChromeOS::ExtensionTabUtilDelegateChromeOS() {}

ExtensionTabUtilDelegateChromeOS::~ExtensionTabUtilDelegateChromeOS() {}

ExtensionTabUtil::ScrubTabBehaviorType
ExtensionTabUtilDelegateChromeOS::GetScrubTabBehavior(
    const Extension* extension) {
  if (!profiles::ArePublicSessionRestrictionsEnabled() ||
      chromeos::DeviceLocalAccountManagementPolicyProvider::IsWhitelisted(
          extension->id())) {
    return ExtensionTabUtil::kDontScrubTab;
  }

  return ExtensionTabUtil::kScrubTabUrlToOrigin;
}

}  // namespace extensions

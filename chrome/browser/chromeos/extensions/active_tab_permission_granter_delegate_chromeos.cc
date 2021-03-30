// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/active_tab_permission_granter_delegate_chromeos.h"

#include "chrome/browser/profiles/profiles_state.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"

namespace extensions {

namespace {

permission_helper::RequestResolvedCallback* g_resolved_callback;

}

ActiveTabPermissionGranterDelegateChromeOS::
    ActiveTabPermissionGranterDelegateChromeOS() {}

ActiveTabPermissionGranterDelegateChromeOS::
    ~ActiveTabPermissionGranterDelegateChromeOS() {}

// static
void ActiveTabPermissionGranterDelegateChromeOS::
    SetRequestResolvedCallbackForTesting(
        permission_helper::RequestResolvedCallback* callback) {
  g_resolved_callback = callback;
}

bool ActiveTabPermissionGranterDelegateChromeOS::ShouldGrantActiveTabOrPrompt(
    const Extension* extension,
    content::WebContents* web_contents) {
  permission_helper::RequestResolvedCallback callback;
  if (g_resolved_callback)
    callback = std::move(*g_resolved_callback);

  if (!profiles::ArePublicSessionRestrictionsEnabled()) {
    if (callback)
      std::move(callback).Run({mojom::APIPermissionID::kActiveTab});
    return true;
  }

  bool already_handled = permission_helper::HandlePermissionRequest(
      *extension, {mojom::APIPermissionID::kActiveTab}, web_contents,
      std::move(callback), permission_helper::PromptFactory());

  return already_handled && permission_helper::PermissionAllowed(
                                extension, mojom::APIPermissionID::kActiveTab);
}

}  // namespace extensions

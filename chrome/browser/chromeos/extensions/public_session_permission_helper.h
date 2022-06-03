// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PUBLIC_SESSION_PERMISSION_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PUBLIC_SESSION_PERMISSION_HELPER_H_

#include <memory>
#include <set>

#include "base/callback_forward.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"

class ExtensionInstallPrompt;

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

namespace permission_helper {

using RequestResolvedCallback =
    base::OnceCallback<void(const PermissionIDSet&)>;
using PromptFactory =
    base::OnceCallback<std::unique_ptr<ExtensionInstallPrompt>(
        content::WebContents*)>;

// In Public Sessions, extensions (and apps) are force-installed by admin policy
// so the user does not get a chance to review the permissions for these
// extensions. This is not acceptable from a security/privacy standpoint, so
// when an extension uses one of the sensitive APIs for the first time, we show
// the user a dialog where they can choose whether to allow the extension access
// to the API.
//
// This function sets up the prompt asking the user for additional
// permission(s), handles the result, caches it, and then runs the callback with
// the allowed permissions as the argument. It returns true if this
// permission(s) is already resolved, and false otherwise.
//
// The user will be prompted about a certain permission only once, and that
// choice will be cached and used in any subsequent requests that use the same
// permission. If a request comes for a permission that is currently being
// prompted, its callback will be queued up to be invoked when the prompt is
// resolved.
//
// Caller must ensure that web_contents is valid. Must be called on UI thread.
//
// Callback can be null (permission_helper::RequestResolvedCallback()), in which
// case it's not invoked but the permission prompt is still shown.
//
// Passing in a null prompt_factory (permission_helper::PromptFactory())
// callback gets the default behaviour (ie. it is is used only for tests).
bool HandlePermissionRequest(const Extension& extension,
                             const PermissionIDSet& requested_permissions,
                             content::WebContents* web_contents,
                             RequestResolvedCallback callback,
                             PromptFactory prompt_factory);

// Returns true if user granted this permission to the extension.
bool PermissionAllowed(const Extension* extension,
                       mojom::APIPermissionID permission);

// Used to completely reset state in between tests.
void ResetPermissionsForTesting();

}  // namespace permission_helper
}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PUBLIC_SESSION_PERMISSION_HELPER_H_

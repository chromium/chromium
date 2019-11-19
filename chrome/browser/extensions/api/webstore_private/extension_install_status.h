// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_EXTENSION_INSTALL_STATUS_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_EXTENSION_INSTALL_STATUS_H_

#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {

enum ExtensionInstallStatus {
  // Extension is blocked by policy but can be requested.
  kCanRequest,
  // Extension install request has been sent and is waiting to be reviewed.
  kRequestPending,
  // Extension is blocked by policy and can not be requested.
  kBlockedByPolicy,
  // Extension is not installed and has not not been blocked by policy.
  kInstallable,
  // Extension has been installed and it's enabled.
  kEnabled,
  // Extension has been installed but it's disabled and not blocked by policy.
  kDisabled,
  // Extension has been installed but it's terminated.
  kTerminated,
  // Extension is blacklisted.
  kBlacklisted
};

// Returns the Extension install status for an Chrome web store extension with
// |extension_id| in |profile|.
ExtensionInstallStatus GetWebstoreExtensionInstallStatus(
    const ExtensionId& extension_id,
    Profile* profile);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_EXTENSION_INSTALL_STATUS_H_

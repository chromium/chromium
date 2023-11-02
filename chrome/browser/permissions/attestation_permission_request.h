// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_ATTESTATION_PERMISSION_REQUEST_H_
#define CHROME_BROWSER_PERMISSIONS_ATTESTATION_PERMISSION_REQUEST_H_

#include "base/callback_forward.h"

namespace permissions {
class PermissionRequest;
}

namespace url {
class Origin;
}

// Returns a |permissions::PermissionRequest| that asks the user to consent to
// sending identifying information about their security key. The |origin|
// argument is used to identify the origin that is requesting the permission,
// and only the authority part of the URL is used. The caller doesn't take
// ownership of the returned object because the standard pattern for
// PermissionRequests is that they delete themselves once complete.
permissions::PermissionRequest* NewAttestationPermissionRequest(
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback);

// Returns a |permissions::PermissionRequest| that asks the user to consent to
// |origin| making a U2F API request. The caller doesn't take ownership of the
// returned object because the standard pattern for PermissionRequests is that
// they delete themselves once complete.
permissions::PermissionRequest* NewU2fApiPermissionRequest(
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback);

#endif  // CHROME_BROWSER_PERMISSIONS_ATTESTATION_PERMISSION_REQUEST_H_

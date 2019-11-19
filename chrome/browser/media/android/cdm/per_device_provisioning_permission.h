// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_CDM_PER_DEVICE_PROVISIONING_PERMISSION_H_
#define CHROME_BROWSER_MEDIA_ANDROID_CDM_PER_DEVICE_PROVISIONING_PERMISSION_H_

#include "base/callback.h"

namespace content {
class RenderFrameHost;
}

// Background: Normally MediaDrmBridge must use per-origin provisioning if it's
// supported by the Android version. Otherwise per-device provisioning is used.
// See MediaDrmBridge::IsPerOriginProvisioningSupported(). Identifiers involved
// in provisioning in both cases are covered by "Protected Content" content
// setting and ProtectedMediaIdentifierPermissionContext, with different default
// settings and permission prompt strings in each case. However, it is possible
// that per-origin provisioning is supported, but does not work, e.g. cannot
// access the provisioning server. In some of these cases, falling back to
// per-device provisioning may work. Since per-device provisioning has different
// privacy implications than per-origin provisioning, we need to get user's
// permission before trying to fall back. This function helps get user
// permission in this case. This function should not be called when per-origin
// provisioning is NOT supported, in which case per-device provisioning should
// be used and is already covered by ProtectedMediaIdentifierPermissionContext.
// For more details, see https://crbug.com/917527.

// Requests permission to allow MediaDrmBridge to use per-device provisioning.
// The |callback| is guaranteed to be called with whether the permission was
// allowed by the user. The decision is not persisted and does not affect any
// persisted settings, e.g. content settings. However, the last response is
// saved in memory, and if another request for the same origin happens within 15
// minutes, the previous response is used.
void RequestPerDeviceProvisioningPermission(
    content::RenderFrameHost* render_frame_host,
    base::OnceCallback<void(bool)> callback);

#endif  // CHROME_BROWSER_MEDIA_ANDROID_CDM_PER_DEVICE_PROVISIONING_PERMISSION_H_

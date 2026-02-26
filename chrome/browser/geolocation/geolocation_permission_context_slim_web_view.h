// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_SLIM_WEB_VIEW_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_SLIM_WEB_VIEW_H_

#include <optional>

#include "base/functional/callback_forward.h"

class GURL;

namespace content {
struct PermissionResult;
}

namespace permissions {
class PermissionRequestID;
}

class GeolocationPermissionContextSlimWebView {
 public:
  // Potentially handles a permission request. Returns whether the permission
  // request was handled and the callback was consumed.
  static bool DecidePermission(
      const permissions::PermissionRequestID& request_id,
      const GURL& requesting_frame,
      bool user_gesture,
      base::OnceCallback<void(content::PermissionResult)>* callback);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_SLIM_WEB_VIEW_H_

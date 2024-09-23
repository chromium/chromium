// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CONTEXT_PERMISSIONS_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CONTEXT_PERMISSIONS_DELEGATE_H_

#include "components/permissions/permission_manager.h"

namespace android_webview {

// Delegate interface to look up permissions in `AwBrowserContext`.
// This interface exists to encapsulate the parts of AwBrowserContext that is
// depended upon by AwPermissionManager, so it can be easily mocked out in
// tests, and to break any circular dependencies between AwBrowserContext and
// AwPermissionManager.
class AwContextPermissionsDelegate {
 public:
  virtual ~AwContextPermissionsDelegate() = default;
  virtual PermissionStatus GetGeolocationPermission(
      const GURL& requesting_origin) const;

 protected:
  AwContextPermissionsDelegate() = default;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTEXT_PERMISSIONS_DELEGATE_H_

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_PERMISSION_REQUEST_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_PERMISSION_REQUEST_DELEGATE_H_

#include "android_webview/browser/permission/permission_callback.h"
#include "base/functional/callback_forward.h"

class GURL;

namespace url {
class Origin;
}

namespace android_webview {

// Delegate interface to handle the permission requests from |BrowserContext|.
class AwBrowserPermissionRequestDelegate {
 public:
  // Returns the AwBrowserPermissionRequestDelegate instance associated with
  // the given render_process_id and render_frame_id, or NULL.
  static AwBrowserPermissionRequestDelegate* FromID(int render_process_id,
                                                    int render_frame_id);

  virtual void RequestProtectedMediaIdentifierPermission(
      const GURL& origin,
      PermissionCallback callback) = 0;

  virtual void CancelProtectedMediaIdentifierPermissionRequests(
      const GURL& origin) = 0;

  virtual void RequestGeolocationPermission(const GURL& origin,
                                            PermissionCallback callback) = 0;

  virtual void CancelGeolocationPermissionRequests(const GURL& origin) = 0;

  virtual void RequestMIDISysexPermission(const GURL& origin,
                                          PermissionCallback callback) = 0;

  virtual void CancelMIDISysexPermissionRequests(const GURL& origin) = 0;

  virtual void RequestStorageAccess(const url::Origin& top_level_origin,
                                    PermissionCallback callback) = 0;

 protected:
  AwBrowserPermissionRequestDelegate() {}
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_PERMISSION_REQUEST_DELEGATE_H_

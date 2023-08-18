// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PERMISSION_AW_PERMISSION_REQUEST_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_PERMISSION_AW_PERMISSION_REQUEST_DELEGATE_H_

#include <stdint.h>

#include "url/gurl.h"

namespace android_webview {

// The delegate interface to be implemented for a specific permission request.
// Lifetime: Temporary
class AwPermissionRequestDelegate {
 public:
  AwPermissionRequestDelegate();

  AwPermissionRequestDelegate(const AwPermissionRequestDelegate&) = delete;
  AwPermissionRequestDelegate& operator=(const AwPermissionRequestDelegate&) =
      delete;

  virtual ~AwPermissionRequestDelegate();

  // Get the origin which initiated the permission request.
  virtual const GURL& GetOrigin() = 0;

  // Get the resources the origin wanted to access.
  virtual int64_t GetResources() = 0;

  // Notify the permission request is allowed or not.
  virtual void NotifyRequestResult(bool allowed) = 0;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PERMISSION_AW_PERMISSION_REQUEST_DELEGATE_H_

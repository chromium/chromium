// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PERMISSION_PERMISSION_REQUEST_HANDLER_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_PERMISSION_PERMISSION_REQUEST_HANDLER_CLIENT_H_

#include "base/android/scoped_java_ref.h"

namespace android_webview {

class AwPermissionRequest;

class PermissionRequestHandlerClient {
 public:
  PermissionRequestHandlerClient();
  virtual ~PermissionRequestHandlerClient();

  virtual void OnPermissionRequest(
      base::android::ScopedJavaLocalRef<jobject> java_request,
      AwPermissionRequest* request) = 0;
  virtual void OnPermissionRequestCanceled(AwPermissionRequest* request) = 0;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PERMISSION_PERMISSION_REQUEST_HANDLER_CLIENT_H_

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/permission/simple_permission_request.h"

#include "android_webview/browser/permission/aw_permission_request.h"
#include "base/functional/callback.h"

namespace android_webview {

SimplePermissionRequest::SimplePermissionRequest(const GURL& origin,
                                                 int64_t resources,
                                                 PermissionCallback callback)
    : origin_(origin), resources_(resources), callback_(std::move(callback)) {}

SimplePermissionRequest::~SimplePermissionRequest() {}

void SimplePermissionRequest::NotifyRequestResult(bool allowed) {
  std::move(callback_).Run(allowed);
}

const GURL& SimplePermissionRequest::GetOrigin() {
  return origin_;
}

int64_t SimplePermissionRequest::GetResources() {
  return resources_;
}

}  // namespace android_webview

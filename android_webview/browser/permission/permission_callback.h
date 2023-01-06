// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PERMISSION_PERMISSION_CALLBACK_H_
#define ANDROID_WEBVIEW_BROWSER_PERMISSION_PERMISSION_CALLBACK_H_

#include "base/functional/callback.h"

namespace android_webview {

// Callback for permission requests.
using PermissionCallback = base::OnceCallback<void(bool)>;

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PERMISSION_PERMISSION_CALLBACK_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_contents_lifecycle_notifier.h"

#include "android_webview/browser_jni_headers/AwContentsLifecycleNotifier_jni.h"

using base::android::AttachCurrentThread;

namespace android_webview {

// static
void AwContentsLifecycleNotifier::OnWebViewCreated() {
  Java_AwContentsLifecycleNotifier_onWebViewCreated(AttachCurrentThread());
}

// static
void AwContentsLifecycleNotifier::OnWebViewDestroyed() {
  Java_AwContentsLifecycleNotifier_onWebViewDestroyed(AttachCurrentThread());
}

}  // namespace android_webview

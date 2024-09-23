// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/router/chrome_media_router_client.h"

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ChromeMediaRouterClient_jni.h"

namespace media_router {

void InitChromeMediaRouterJavaClient() {
  Java_ChromeMediaRouterClient_initialize(base::android::AttachCurrentThread());
}

}  // namespace media_router

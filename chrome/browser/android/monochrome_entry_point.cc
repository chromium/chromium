// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/webview_jni_onload.h"
#include "base/android/base_jni_init.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/notreached.h"
#include "chrome/app/android/chrome_jni_onload.h"

bool NativeInitializationHook(
    base::android::LibraryProcessType library_process_type) {
  switch (library_process_type) {
    case base::android::PROCESS_WEBVIEW:
    case base::android::PROCESS_WEBVIEW_CHILD:
      return android_webview::OnJNIOnLoadInit();
    case base::android::PROCESS_BROWSER:
    case base::android::PROCESS_CHILD:
      return android::OnJNIOnLoadInit();

    case base::android::PROCESS_WEBVIEW_NONEMBEDDED:
      return base::android::OnJNIOnLoadInit();

    default:
      NOTREACHED();
  }
}

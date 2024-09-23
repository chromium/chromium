// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/webview_jni_onload.h"
#include "base/android/base_jni_onload.h"
#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/logging.h"

namespace {

bool NativeInit(base::android::LibraryProcessType library_process_type) {
  switch (library_process_type) {
    case base::android::PROCESS_WEBVIEW:
    case base::android::PROCESS_WEBVIEW_CHILD:
      return android_webview::OnJNIOnLoadInit();

    case base::android::PROCESS_WEBVIEW_NONEMBEDDED:
      return base::android::OnJNIOnLoadInit();

    case base::android::PROCESS_CHILD:
      LOG(FATAL) << "WebView cannot be started with a child process type.";
    case base::android::PROCESS_BROWSER:
      LOG(FATAL) << "WebView cannot be started with a browser process type.";

    default:
      NOTREACHED();
  }
}

}  // namespace

// This is called by the VM when the shared library is first loaded.
// Most of the initialization is done in LibraryLoadedOnMainThread(), not here.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  base::android::SetNativeInitializationHook(&NativeInit);
  return JNI_VERSION_1_4;
}

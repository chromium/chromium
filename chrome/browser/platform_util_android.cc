// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/notreached.h"
#include "chrome/browser/platform_util.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/util/jni_headers/PlatformUtil_jni.h"

using base::android::ScopedJavaLocalRef;

namespace platform_util {

// TODO: crbug/115682 to track implementation of the following methods.

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  NOTIMPLEMENTED();
}

void OpenItem(Profile* profile,
              const base::FilePath& full_path,
              OpenItemType item_type,
              OpenOperationCallback callback) {
  NOTIMPLEMENTED();
}

void OpenExternal(const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_url =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  Java_PlatformUtil_launchExternalProtocol(env, j_url);
}

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  NOTIMPLEMENTED();
  return view->GetWindowAndroid();
}

gfx::NativeView GetParent(gfx::NativeView view) {
  NOTIMPLEMENTED();
  return view;
}

bool IsWindowActive(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return false;
}

void ActivateWindow(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
}

bool IsVisible(gfx::NativeView view) {
  NOTIMPLEMENTED();
  return true;
}

} // namespace platform_util

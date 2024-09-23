// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/window_android.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/snapshot/snapshot.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ScreenshotTask_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using jni_zero::AttachCurrentThread;
using ui::WindowAndroid;

namespace chrome {
namespace android {

void JNI_ScreenshotTask_SnapshotCallback(
    JNIEnv* env,
    const JavaRef<jobject>& callback,
    scoped_refptr<base::RefCountedMemory> png_data) {
  if (png_data.get()) {
    size_t size = png_data->size();
    ScopedJavaLocalRef<jbyteArray> jbytes(env, env->NewByteArray(size));
    env->SetByteArrayRegion(jbytes.obj(), 0, size, (jbyte*)png_data->front());
    Java_ScreenshotTask_onBytesReceived(env, callback, jbytes);
  } else {
    Java_ScreenshotTask_onBytesReceived(env, callback, nullptr);
  }
}

void JNI_ScreenshotTask_GrabWindowSnapshotAsync(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcallback,
    const JavaParamRef<jobject>& jwindow_android,
    jint window_width,
    jint window_height) {
  ui::WindowAndroid* window_android =
      ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  gfx::Rect window_bounds(window_width, window_height);
  ui::GrabWindowSnapshotAsPNG(
      window_android, window_bounds,
      base::BindOnce(&JNI_ScreenshotTask_SnapshotCallback, env,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

}  // namespace android
}  // namespace chrome

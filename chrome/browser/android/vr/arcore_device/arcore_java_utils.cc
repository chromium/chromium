// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_java_utils.h"

#include "base/android/jni_string.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/vr/arcore_device/arcore_device.h"
#include "chrome/browser/android/vr/arcore_device/arcore_shim.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "jni/ArCoreJavaUtils_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace vr {

ArCoreJavaUtils::ArCoreJavaUtils(device::ArCoreDevice* arcore_device)
    : arcore_device_(arcore_device) {
  DCHECK(arcore_device_);

  JNIEnv* env = AttachCurrentThread();
  if (!env)
    return;
  ScopedJavaLocalRef<jobject> j_arcore_java_utils =
      Java_ArCoreJavaUtils_create(env, (jlong)this);
  if (j_arcore_java_utils.is_null())
    return;
  j_arcore_java_utils_.Reset(j_arcore_java_utils);
}

ArCoreJavaUtils::~ArCoreJavaUtils() {
  JNIEnv* env = AttachCurrentThread();
  Java_ArCoreJavaUtils_onNativeDestroy(env, j_arcore_java_utils_);
}

void ArCoreJavaUtils::OnRequestInstallSupportedArCoreCanceled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  // TODO(crbug.com/893348): don't reach back into arcore device like this.
  arcore_device_->OnRequestInstallSupportedArCoreCanceled();
}

bool ArCoreJavaUtils::ShouldRequestInstallArModule() {
  return Java_ArCoreJavaUtils_shouldRequestInstallArModule(
      AttachCurrentThread(), j_arcore_java_utils_);
}

void ArCoreJavaUtils::RequestInstallArModule() {
  Java_ArCoreJavaUtils_requestInstallArModule(AttachCurrentThread(),
                                              j_arcore_java_utils_);
}

bool ArCoreJavaUtils::ShouldRequestInstallSupportedArCore() {
  JNIEnv* env = AttachCurrentThread();
  return Java_ArCoreJavaUtils_shouldRequestInstallSupportedArCore(
      env, j_arcore_java_utils_);
}

void ArCoreJavaUtils::RequestInstallSupportedArCore(int render_process_id,
                                                    int render_frame_id) {
  DCHECK(ShouldRequestInstallSupportedArCore());

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  DCHECK(render_frame_host);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab_android);

  base::android::ScopedJavaLocalRef<jobject> j_tab_android =
      tab_android->GetJavaObject();
  DCHECK(!j_tab_android.is_null());

  JNIEnv* env = AttachCurrentThread();
  Java_ArCoreJavaUtils_requestInstallSupportedArCore(env, j_arcore_java_utils_,
                                                     j_tab_android);
}

void ArCoreJavaUtils::OnRequestInstallArModuleResult(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    bool success) {
  // TODO(crbug.com/893348): don't reach back into arcore device like this.
  arcore_device_->OnRequestInstallArModuleResult(success);
}

bool ArCoreJavaUtils::EnsureLoaded() {
  JNIEnv* env = AttachCurrentThread();
  if (!Java_ArCoreJavaUtils_shouldLoadArCoreSdk(env))
    return false;

  // TODO(crbug.com/884780): Allow loading the ARCore shim by name instead of by
  // absolute path.
  ScopedJavaLocalRef<jstring> java_path =
      Java_ArCoreJavaUtils_getArCoreShimLibraryPath(env);
  return LoadArCoreSdk(base::android::ConvertJavaStringToUTF8(env, java_path));
}

ScopedJavaLocalRef<jobject> ArCoreJavaUtils::GetApplicationContext() {
  JNIEnv* env = AttachCurrentThread();
  return Java_ArCoreJavaUtils_getApplicationContext(env);
}

}  // namespace vr

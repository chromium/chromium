// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/bluetooth/android/jni_headers/BluetoothBridge_jni.h"

jboolean JNI_BluetoothBridge_IsWebContentsConnectedToBluetoothDevice(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  return web_contents->IsConnectedToBluetoothDevice();
}

jboolean JNI_BluetoothBridge_IsWebContentsScanningForBluetoothDevices(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  return web_contents->IsScanningForBluetoothDevices();
}

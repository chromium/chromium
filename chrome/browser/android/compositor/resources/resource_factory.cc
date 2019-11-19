// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/chrome_jni_headers/ResourceFactory_jni.h"
#include "chrome/browser/android/compositor/resources/toolbar_resource.h"

using base::android::JavaParamRef;

namespace android {

jlong JNI_ResourceFactory_CreateToolbarContainerResource(
    JNIEnv* env,
    jint toolbar_left,
    jint toolbar_top,
    jint toolbar_right,
    jint toolbar_bottom,
    jint location_bar_left,
    jint location_bar_top,
    jint location_bar_right,
    jint location_bar_bottom,
    jint shadow_height) {
  gfx::Rect toolbar_rect(toolbar_left, toolbar_top,
                         toolbar_right - toolbar_left,
                         toolbar_bottom - toolbar_top);
  gfx::Rect location_bar_content_rect(location_bar_left, location_bar_top,
                                      location_bar_right - location_bar_left,
                                      location_bar_bottom - location_bar_top);
  return reinterpret_cast<intptr_t>(new ToolbarResource(
      toolbar_rect, location_bar_content_rect, shadow_height));
}

}  // namespace android

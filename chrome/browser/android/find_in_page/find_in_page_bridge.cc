// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/find_in_page/find_in_page_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/FindInPageBridge_jni.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_types.h"
#include "content/public/browser/web_contents.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

FindInPageBridge::FindInPageBridge(JNIEnv* env,
                                   const JavaRef<jobject>& obj,
                                   const JavaRef<jobject>& j_web_contents)
    : weak_java_ref_(env, obj) {
  web_contents_ = content::WebContents::FromJavaWebContents(j_web_contents);
}

void FindInPageBridge::Destroy(JNIEnv*, const JavaParamRef<jobject>&) {
  delete this;
}

void FindInPageBridge::StartFinding(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    const JavaParamRef<jstring>& search_string,
                                    jboolean forward_direction,
                                    jboolean case_sensitive) {
  FindTabHelper::FromWebContents(web_contents_)->
      StartFinding(
          base::android::ConvertJavaStringToUTF16(env, search_string),
          forward_direction,
          case_sensitive);
}

void FindInPageBridge::StopFinding(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jboolean clearSelection) {
  FindTabHelper::FromWebContents(web_contents_)
      ->StopFinding(clearSelection ? FindOnPageSelectionAction::kClear
                                   : FindOnPageSelectionAction::kKeep);
}

ScopedJavaLocalRef<jstring> FindInPageBridge::GetPreviousFindText(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF16ToJavaString(
      env, FindTabHelper::FromWebContents(web_contents_)->previous_find_text());
}

void FindInPageBridge::RequestFindMatchRects(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             jint current_version) {
  FindTabHelper::FromWebContents(web_contents_)->
      RequestFindMatchRects(current_version);
}

void FindInPageBridge::ActivateNearestFindResult(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jfloat x,
    jfloat y) {
  FindTabHelper::FromWebContents(web_contents_)->
      ActivateNearestFindResult(x, y);
}

void FindInPageBridge::ActivateFindInPageResultForAccessibility(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  FindTabHelper::FromWebContents(web_contents_)->
      ActivateFindInPageResultForAccessibility();
}

// static
static jlong JNI_FindInPageBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_web_contents) {
  FindInPageBridge* bridge = new FindInPageBridge(env, obj, j_web_contents);
  return reinterpret_cast<intptr_t>(bridge);
}

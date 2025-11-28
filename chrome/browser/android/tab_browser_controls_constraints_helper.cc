// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_browser_controls_constraints_helper.h"

#include "cc/input/android/offset_tag_android.h"
#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/input/browser_controls_state.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabBrowserControlsConstraintsHelper_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;

TabBrowserControlsConstraintsHelper::TabBrowserControlsConstraintsHelper(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj)
    : jobj_(env, obj) {}

TabBrowserControlsConstraintsHelper::~TabBrowserControlsConstraintsHelper() =
    default;

void TabBrowserControlsConstraintsHelper::OnDestroyed(JNIEnv* env) {
  delete this;
}

void TabBrowserControlsConstraintsHelper::UpdateState(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    jint constraints,
    jint current,
    jboolean animate,
    const JavaParamRef<jobject>& joffset_tag_modifications) {
  cc::BrowserControlsState constraints_state =
      static_cast<cc::BrowserControlsState>(constraints);
  cc::BrowserControlsState current_state =
      static_cast<cc::BrowserControlsState>(current);

  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  if (web_contents == nullptr) {
    return;
  }

  cc::BrowserControlsOffsetTagModifications offset_tag_modifications =
      cc::android::FromJavaBrowserControlsOffsetTagModifications(
          env, joffset_tag_modifications);
  web_contents->UpdateBrowserControlsState(constraints_state, current_state,
                                           animate, offset_tag_modifications);
}

static jlong JNI_TabBrowserControlsConstraintsHelper_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(
      new TabBrowserControlsConstraintsHelper(env, obj));
}

DEFINE_JNI(TabBrowserControlsConstraintsHelper)

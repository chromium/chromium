// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_browser_controls_state.h"

#include "chrome/android/chrome_jni_headers/TabBrowserControlsState_jni.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_controls_state.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;

TabBrowserControlsState::TabBrowserControlsState(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj)
    : jobj_(env, obj) {}

TabBrowserControlsState::~TabBrowserControlsState() = default;

void TabBrowserControlsState::OnDestroyed(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  delete this;
}

void TabBrowserControlsState::UpdateState(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents,
    jint constraints,
    jint current,
    jboolean animate) {
  content::BrowserControlsState constraints_state =
      static_cast<content::BrowserControlsState>(constraints);
  content::BrowserControlsState current_state =
      static_cast<content::BrowserControlsState>(current);

  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  web_contents->GetMainFrame()->UpdateBrowserControlsState(
      constraints_state, current_state, animate);

  if (web_contents->ShowingInterstitialPage()) {
    web_contents->GetInterstitialPage()
        ->GetMainFrame()
        ->UpdateBrowserControlsState(constraints_state, current_state, animate);
  }
}

static jlong JNI_TabBrowserControlsState_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new TabBrowserControlsState(env, obj));
}

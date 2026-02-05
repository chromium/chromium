// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/android/offset_tag_android.h"
#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/input/browser_controls_state.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabBrowserControlsConstraintsHelper_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;

static void JNI_TabBrowserControlsConstraintsHelper_UpdateState(
    JNIEnv* env,
    content::WebContents* web_contents,
    int32_t constraints,
    int32_t current,
    bool animate,
    const JavaRef<jobject>& joffset_tag_modifications) {
  cc::BrowserControlsState constraints_state =
      static_cast<cc::BrowserControlsState>(constraints);
  cc::BrowserControlsState current_state =
      static_cast<cc::BrowserControlsState>(current);

  if (web_contents == nullptr) {
    return;
  }

  cc::BrowserControlsOffsetTagModifications offset_tag_modifications =
      cc::android::FromJavaBrowserControlsOffsetTagModifications(
          env, joffset_tag_modifications);
  web_contents->UpdateBrowserControlsState(constraints_state, current_state,
                                           animate, offset_tag_modifications);
}

DEFINE_JNI(TabBrowserControlsConstraintsHelper)

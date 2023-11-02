// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_VR_INPUT_CONNECTION_H_
#define CHROME_BROWSER_ANDROID_VR_VR_INPUT_CONNECTION_H_

#include <jni.h>
#include <queue>

#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "chrome/browser/vr/platform_input_handler.h"
#include "chrome/browser/vr/text_edit_action.h"

namespace content {
class WebContents;
}  // namespace content

namespace vr {

class VrInputConnection {
 public:
  explicit VrInputConnection(content::WebContents* web_contents);

  VrInputConnection(const VrInputConnection&) = delete;
  VrInputConnection& operator=(const VrInputConnection&) = delete;

  ~VrInputConnection();

  void OnKeyboardEdit(const TextEdits& edits);
  void SubmitInput();
  void RequestTextState(TextStateUpdateCallback callback);
  void UpdateTextState(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jstring jtext);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
  std::queue<vr::TextStateUpdateCallback> text_state_update_callbacks_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_VR_INPUT_CONNECTION_H_

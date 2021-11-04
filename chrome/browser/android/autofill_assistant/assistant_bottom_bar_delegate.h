// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_BOTTOM_BAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_BOTTOM_BAR_DELEGATE_H_

#include "base/android/scoped_java_ref.h"

namespace autofill_assistant {
class UiControllerAndroid;
// Delegate class for the assistant bottom bar.
class AssistantBottomBarDelegate {
 public:
  explicit AssistantBottomBarDelegate(UiControllerAndroid* ui_controller);
  ~AssistantBottomBarDelegate();

  // Returns true if the back button press was handled by Autofill Assistant.
  bool OnBackButtonClicked(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jcaller);

  void OnBottomSheetClosedWithSwipe(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaObject();

 private:
  UiControllerAndroid* ui_controller_;

  // Java-side AssistantBottomBarDelegate object.
  base::android::ScopedJavaGlobalRef<jobject>
      java_assistant_bottom_bar_delegate_;
};
}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_BOTTOM_BAR_DELEGATE_H_

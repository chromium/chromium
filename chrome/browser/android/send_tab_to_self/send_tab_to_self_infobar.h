// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_INFOBAR_H_
#define CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_INFOBAR_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"
#include "components/send_tab_to_self/send_tab_to_self_infobar_delegate.h"

namespace content {
class WebContents;
}

namespace send_tab_to_self {
// Communicates to the user that a tab was shared from another device. See
// SendTabToSelfInfoBar.java for UI specifics, and SendTabToSelfInfobarDelegate
// for behavior specifics.
class SendTabToSelfInfoBar : public InfoBarAndroid {
 public:
  ~SendTabToSelfInfoBar() override;
  // |delegate| must remain alive while showing this info bar.
  static void ShowInfoBar(
      content::WebContents* web_contents,
      std::unique_ptr<SendTabToSelfInfoBarDelegate> delegate);

 private:
  explicit SendTabToSelfInfoBar(
      std::unique_ptr<SendTabToSelfInfoBarDelegate> delegate);
  // InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;
  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj) override;
  void ProcessButton(int action) override;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfInfoBar);
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_INFOBAR_H_

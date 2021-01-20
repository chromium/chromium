// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_AUTO_SIGNIN_FIRST_RUN_DIALOG_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_AUTO_SIGNIN_FIRST_RUN_DIALOG_ANDROID_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/ui/passwords/manage_passwords_state.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

// Native counterpart for the android dialog which informs user about auto
// sign-in feature and allows to turn it off.
class AutoSigninFirstRunDialogAndroid : public content::WebContentsObserver {
 public:
  explicit AutoSigninFirstRunDialogAndroid(content::WebContents* web_contents);

  void Destroy(JNIEnv* env, jobject obj);

  void ShowDialog();

  // Closes the dialog and propagates that no credentials was chosen.
  void CancelDialog(JNIEnv* env, jobject obj);

  // Opens new tab with page which explains the Smart Lock branding.
  void OnLinkClicked(JNIEnv* env, jobject obj);

  // Records the user decision to use auto sign-in feature.
  void OnOkClicked(JNIEnv* env, jobject obj);

  // Opts user out of the auto sign-in feature.
  void OnTurnOffClicked(JNIEnv* env, jobject obj);

  // content::WebContentsObserver overrides:
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  ~AutoSigninFirstRunDialogAndroid() override;

  content::WebContents* web_contents_;

  base::android::ScopedJavaGlobalRef<jobject> dialog_jobject_;

  DISALLOW_COPY_AND_ASSIGN(AutoSigninFirstRunDialogAndroid);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_AUTO_SIGNIN_FIRST_RUN_DIALOG_ANDROID_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_BLOCKED_DIALOG_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_BLOCKED_DIALOG_CONTROLLER_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/permissions/quiet_permission_prompt_model_android.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_ui_selector.h"

namespace content {
class WebContents;
}

// Controller for dialog triggered by the user clicking on the "manage"
// button in the Messages 2.0 flavor of quiet notification permission
// prompt (see: NotificationBlockedMessageDelegate).
class PermissionBlockedDialogController {
 public:
  // The implementor of this interface may destroy the
  // `NotificationBlockedDialogController` instance immediately in
  // these callbacks.
  class Delegate {
   public:
    virtual void OnContinueBlocking() = 0;
    virtual void OnAllowForThisSite() = 0;
    virtual void OnLearnMoreClicked() = 0;
    virtual void OnOpenedSettings() = 0;
    virtual void OnDialogDismissed() = 0;
    virtual ContentSettingsType GetContentSettingsType() = 0;
  };

  // Both the `delegate` and `web_contents` should outlive `this`.
  PermissionBlockedDialogController(Delegate* delegate,
                                      content::WebContents* web_contents_);
  ~PermissionBlockedDialogController();

  void ShowDialog(
      permissions::PermissionUiSelector::QuietUiReason quiet_ui_reason);
  void DismissDialog();

  void OnPrimaryButtonClicked(JNIEnv* env);
  void OnNegativeButtonClicked(JNIEnv* env);
  void OnLearnMoreClicked(JNIEnv* env);
  void OnDialogDismissed(JNIEnv* env);

 private:
  // Returns either the fully initialized java counterpart of this bridge or
  // a is_null() reference if the creation failed. By using this method, the
  // bridge will try to recreate the java object if it failed previously (e.g.
  // because there was no native window available).
  base::android::ScopedJavaGlobalRef<jobject> GetOrCreateJavaObject();

  raw_ptr<Delegate> delegate_ = nullptr;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  QuietPermissionPromptModelAndroid prompt_model_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_BLOCKED_DIALOG_CONTROLLER_ANDROID_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ADD_USERNAME_DIALOG_ADD_USERNAME_DIALOG_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ADD_USERNAME_DIALOG_ADD_USERNAME_DIALOG_BRIDGE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/gfx/native_widget_types.h"

class AddUsernameDialogBridge {
 public:
  using DialogAcceptedCallback =
      base::OnceCallback<void(const std::u16string&)>;
  class JniDelegate {
   public:
    JniDelegate();
    JniDelegate(const JniDelegate&) = delete;
    JniDelegate& operator=(const JniDelegate&) = delete;
    virtual ~JniDelegate() = 0;

    virtual void Create(const gfx::NativeWindow window_android,
                        AddUsernameDialogBridge* bridge) = 0;
    virtual void ShowAddUsernameDialog(const std::u16string& password) = 0;
    // Dismisses the displayed dialog. The bridge calls it in the destructor to
    // ensure that the dialog on the java side is dismissed.
    virtual void Dismiss() = 0;
  };

  AddUsernameDialogBridge();
  AddUsernameDialogBridge(
      base::PassKey<class GeneratedPasswordSavedMessageDelegateTest>,
      std::unique_ptr<JniDelegate> jni_delegate);
  ~AddUsernameDialogBridge();
  // Disallow copy and assign.
  AddUsernameDialogBridge(const AddUsernameDialogBridge&) = delete;
  AddUsernameDialogBridge& operator=(const AddUsernameDialogBridge&) = delete;

  void ShowAddUsernameDialog(const gfx::NativeWindow window_android,
                             const std::u16string& password,
                             DialogAcceptedCallback dialog_accepted_callback,
                             base::OnceClosure dialog_dismissed_callback);

  void OnDialogAccepted(JNIEnv* env,
                        const base::android::JavaParamRef<jstring>& username);
  void OnDialogDismissed(JNIEnv* env);

 private:
  std::unique_ptr<JniDelegate> jni_delegate_;
  DialogAcceptedCallback dialog_accepted_callback_;
  base::OnceClosure dialog_dismissed_callback_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ADD_USERNAME_DIALOG_ADD_USERNAME_DIALOG_BRIDGE_H_

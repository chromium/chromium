// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_DIALOG_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_DIALOG_ANDROID_H_

#include <stddef.h>

#include <vector>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "chrome/browser/ui/passwords/manage_passwords_state.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

// Native counterpart for the android dialog which allows users to select
// credentials which will be passed to the web site in order to log in the user.
class AccountChooserDialogAndroid : public content::WebContentsObserver {
 public:
  AccountChooserDialogAndroid(
      content::WebContents* web_contents,
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials,
      const GURL& origin,
      const ManagePasswordsState::CredentialsCallback& callback);

  ~AccountChooserDialogAndroid() override;
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Returns true if the dialog is shown. Otherwise, the instance is deleted.
  bool ShowDialog();

  // Closes the dialog and propagates that no credentials was chosen.
  void CancelDialog(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);

  // Propagates the credentials chosen by the user.
  void OnCredentialClicked(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jint credential_item,
                           jboolean sign_button_clicked);

  // Opens new tab with page which explains the Smart Lock branding.
  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

  // content::WebContentsObserver overrides:
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  void OnDialogCancel();

  const std::vector<std::unique_ptr<autofill::PasswordForm>>&
  local_credentials_forms() const;

  void ChooseCredential(size_t index,
                        password_manager::CredentialType type,
                        bool sign_button_clicked);

  content::WebContents* web_contents_;
  ManagePasswordsState passwords_data_;
  GURL origin_;
  base::android::ScopedJavaGlobalRef<jobject> dialog_jobject_;

  DISALLOW_COPY_AND_ASSIGN(AccountChooserDialogAndroid);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_DIALOG_ANDROID_H_

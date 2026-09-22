// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_ANDROID_TOUCH_TO_FILL_PASSWORD_MANAGER_VIEW_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_ANDROID_TOUCH_TO_FILL_PASSWORD_MANAGER_VIEW_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_password_manager_view.h"

namespace password_manager {
class UiCredential;
}  // namespace password_manager

class TouchToFillPasswordManagerController;

// This class provides an implementation of the TouchToFillPasswordManagerView
// interface and communicates via JNI with its TouchToFillBridge Java
// counterpart.
class TouchToFillPasswordManagerViewImpl
    : public TouchToFillPasswordManagerView {
 public:
  explicit TouchToFillPasswordManagerViewImpl(
      TouchToFillPasswordManagerController* controller);
  ~TouchToFillPasswordManagerViewImpl() override;

  // TouchToFillPasswordManagerView:
  bool Show(const GURL& url,
            IsOriginSecure is_origin_secure,
            base::span<const Credential> credentials,
            int flags) override;
  void OnCredentialSelected(
      const password_manager::UiCredential& credential) override;
  void OnDismiss() override;

  void OnCredentialSelected(JNIEnv* env,
                            const jni_zero::JavaRef<jobject>& credential);
  void OnWebAuthnCredentialSelected(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& credential);
  void OnManagePasswordsSelected(bool passkeys_shown);
  void OnHybridSignInSelected();
  void OnShowCredManSelected();

 private:
  // Returns either true if the java counterpart of this bridge is initialized
  // successfully or false if the creation failed. This method will recreate the
  // java object whenever Show() is called.
  bool RecreateJavaObject();

  raw_ptr<TouchToFillPasswordManagerController> controller_ = nullptr;
  jni_zero::ScopedJavaGlobalRef<jobject> java_object_internal_;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_ANDROID_TOUCH_TO_FILL_PASSWORD_MANAGER_VIEW_IMPL_H_

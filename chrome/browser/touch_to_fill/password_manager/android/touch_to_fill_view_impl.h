// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_ANDROID_TOUCH_TO_FILL_VIEW_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_ANDROID_TOUCH_TO_FILL_VIEW_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_view.h"

namespace password_manager {
class PasskeyCredential;
class UiCredential;
}  // namespace password_manager

class TouchToFillController;

// This class provides an implementation of the TouchToFillView interface and
// communicates via JNI with its TouchToFillBridge Java counterpart.
class TouchToFillViewImpl : public TouchToFillView {
 public:
  explicit TouchToFillViewImpl(TouchToFillController* controller);
  ~TouchToFillViewImpl() override;

  // TouchToFillView:
  bool Show(
      const GURL& url,
      IsOriginSecure is_origin_secure,
      base::span<const password_manager::UiCredential> credentials,
      base::span<const password_manager::PasskeyCredential> passkey_credentials,
      int flags) override;
  void OnCredentialSelected(
      const password_manager::UiCredential& credential) override;
  void OnDismiss() override;

  void OnCredentialSelected(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& credential);
  void OnWebAuthnCredentialSelected(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& credential);
  void OnManagePasswordsSelected(JNIEnv* env, jboolean passkeys_shown);
  void OnHybridSignInSelected(JNIEnv* env);
  void OnShowCredManSelected(JNIEnv* env);
  void OnDismiss(JNIEnv* env);

 private:
  // Returns either true if the java counterpart of this bridge is initialized
  // successfully or false if the creation failed. This method will recreate the
  // java object whenever Show() is called.
  bool RecreateJavaObject();

  raw_ptr<TouchToFillController> controller_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> java_object_internal_;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_ANDROID_TOUCH_TO_FILL_VIEW_IMPL_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_ANDROID_TOUCH_TO_FILL_VIEW_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_ANDROID_TOUCH_TO_FILL_VIEW_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view.h"

namespace gfx {
class Image;
}

class TouchToFillController;

// This class provides an implementation of the TouchToFillView interface and
// communicates via JNI with its TouchToFillBridge Java counterpart.
class TouchToFillViewImpl : public TouchToFillView {
 public:
  explicit TouchToFillViewImpl(TouchToFillController* controller);
  ~TouchToFillViewImpl() override;

  // TouchToFillView:
  void Show(
      const GURL& url,
      IsOriginSecure is_origin_secure,
      base::span<const password_manager::CredentialPair> credentials) override;
  void OnCredentialSelected(
      const password_manager::CredentialPair& credential) override;
  void OnDismiss() override;

  // Called from Java via JNI:
  void FetchFavicon(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_credential_origin,
      const base::android::JavaParamRef<jstring>& j_frame_origin,
      jint desized_size_in_pixel,
      const base::android::JavaParamRef<jobject>& j_callback);
  void OnCredentialSelected(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& credential);
  void OnManagePasswordsSelected(JNIEnv* env);
  void OnDismiss(JNIEnv* env);

 private:
  TouchToFillController* controller_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_ANDROID_TOUCH_TO_FILL_VIEW_IMPL_H_

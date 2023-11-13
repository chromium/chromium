// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_bridge.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_delegate.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

class TouchToFillPasswordGenerationBridgeImpl
    : public TouchToFillPasswordGenerationBridge {
 public:
  TouchToFillPasswordGenerationBridgeImpl();
  TouchToFillPasswordGenerationBridgeImpl(
      const TouchToFillPasswordGenerationBridgeImpl&) = delete;
  TouchToFillPasswordGenerationBridgeImpl& operator=(
      const TouchToFillPasswordGenerationBridgeImpl&) = delete;
  ~TouchToFillPasswordGenerationBridgeImpl() override;

  bool Show(content::WebContents* web_contents,
            PrefService* pref_service,
            TouchToFillPasswordGenerationDelegate* delegate,
            std::u16string password,
            std::string account) override;

  void Hide() override;

  void OnDismissed(JNIEnv* env, bool generated_password_accepted) override;

  void OnGeneratedPasswordAccepted(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& password) override;

  void OnGeneratedPasswordRejected(JNIEnv* env) override;

 private:
  // The corresponding Java TouchToFillCreditCardViewBridge.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  // The `delegate_` is the owner of this bridge, so its lifetime is for sure
  // longer than this bridge's lifetime.
  raw_ptr<TouchToFillPasswordGenerationDelegate> delegate_ = nullptr;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_IMPL_H_

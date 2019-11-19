// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/safety_tip_infobar.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/android/chrome_jni_headers/SafetyTipInfoBar_jni.h"
#include "chrome/browser/reputation/safety_tip_infobar_delegate.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::ScopedJavaLocalRef;

// static
std::unique_ptr<infobars::InfoBar> SafetyTipInfoBar::CreateInfoBar(
    std::unique_ptr<SafetyTipInfoBarDelegate> delegate) {
  return base::WrapUnique(new SafetyTipInfoBar(std::move(delegate)));
}

SafetyTipInfoBar::~SafetyTipInfoBar() {}

SafetyTipInfoBar::SafetyTipInfoBar(
    std::unique_ptr<SafetyTipInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

ScopedJavaLocalRef<jobject> SafetyTipInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  ScopedJavaLocalRef<jstring> ok_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  ScopedJavaLocalRef<jstring> cancel_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  SafetyTipInfoBarDelegate* delegate = GetDelegate();
  ScopedJavaLocalRef<jstring> message_text =
      base::android::ConvertUTF16ToJavaString(env, delegate->GetMessageText());
  ScopedJavaLocalRef<jstring> link_text =
      base::android::ConvertUTF16ToJavaString(env, delegate->GetLinkText());
  ScopedJavaLocalRef<jstring> description_text =
      base::android::ConvertUTF16ToJavaString(env,
                                              delegate->GetDescriptionText());

  ScopedJavaLocalRef<jobject> java_bitmap;
  if (delegate->GetIconId() == infobars::InfoBarDelegate::kNoIconID &&
      !delegate->GetIcon().IsEmpty()) {
    java_bitmap = gfx::ConvertToJavaBitmap(delegate->GetIcon().ToSkBitmap());
  }

  return Java_SafetyTipInfoBar_create(env, GetEnumeratedIconId(), java_bitmap,
                                      message_text, link_text, ok_button_text,
                                      cancel_button_text, description_text);
}

SafetyTipInfoBarDelegate* SafetyTipInfoBar::GetDelegate() {
  return static_cast<SafetyTipInfoBarDelegate*>(delegate());
}

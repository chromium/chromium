// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/known_interception_disclosure_infobar.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/KnownInterceptionDisclosureInfoBar_jni.h"

using base::android::ScopedJavaLocalRef;

// static
std::unique_ptr<infobars::InfoBar>
KnownInterceptionDisclosureInfoBar::CreateInfoBar(
    std::unique_ptr<KnownInterceptionDisclosureInfoBarDelegate> delegate) {
  return base::WrapUnique(
      new KnownInterceptionDisclosureInfoBar(std::move(delegate)));
}

KnownInterceptionDisclosureInfoBar::KnownInterceptionDisclosureInfoBar(
    std::unique_ptr<KnownInterceptionDisclosureInfoBarDelegate> delegate)
    : infobars::ConfirmInfoBar(std::move(delegate)) {}

ScopedJavaLocalRef<jobject>
KnownInterceptionDisclosureInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  KnownInterceptionDisclosureInfoBarDelegate* delegate = GetDelegate();
  ScopedJavaLocalRef<jstring> ok_button_text =
      base::android::ConvertUTF16ToJavaString(
          env,
          GetTextFor(KnownInterceptionDisclosureInfoBarDelegate::BUTTON_OK));
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
    java_bitmap = gfx::ConvertToJavaBitmap(
        *delegate->GetIcon().Rasterize(nullptr).bitmap());
  }

  return Java_KnownInterceptionDisclosureInfoBar_create(
      env, resource_id_mapper.Run(delegate->GetIconId()), java_bitmap,
      message_text, link_text, ok_button_text, description_text);
}

KnownInterceptionDisclosureInfoBarDelegate*
KnownInterceptionDisclosureInfoBar::GetDelegate() {
  return static_cast<KnownInterceptionDisclosureInfoBarDelegate*>(delegate());
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tip_infobar_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/lookalikes/safety_tip_infobar_delegate_android.h"
#include "chrome/browser/lookalikes/safety_tip_ui.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SafetyTipInfoBar_jni.h"

using base::android::ScopedJavaLocalRef;

// From safety_tip_ui.h
void ShowSafetyTipDialog(
    content::WebContents* web_contents,
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& suggested_url,
    base::OnceCallback<void(SafetyTipInteraction)> close_callback) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  auto delegate = std::make_unique<SafetyTipInfoBarDelegate>(
      safety_tip_status, suggested_url, web_contents,
      std::move(close_callback));
  infobar_manager->AddInfoBar(
      SafetyTipInfoBar::CreateInfoBar(std::move(delegate)));
}

// static
std::unique_ptr<infobars::InfoBar> SafetyTipInfoBar::CreateInfoBar(
    std::unique_ptr<SafetyTipInfoBarDelegate> delegate) {
  return base::WrapUnique(new SafetyTipInfoBar(std::move(delegate)));
}

SafetyTipInfoBar::~SafetyTipInfoBar() {}

SafetyTipInfoBar::SafetyTipInfoBar(
    std::unique_ptr<SafetyTipInfoBarDelegate> delegate)
    : infobars::ConfirmInfoBar(std::move(delegate)) {}

ScopedJavaLocalRef<jobject> SafetyTipInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
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
    java_bitmap = gfx::ConvertToJavaBitmap(
        *delegate->GetIcon().Rasterize(nullptr).bitmap());
  }

  return Java_SafetyTipInfoBar_create(
      env, resource_id_mapper.Run(delegate->GetIconId()), java_bitmap,
      message_text, link_text, ok_button_text, cancel_button_text,
      description_text);
}

SafetyTipInfoBarDelegate* SafetyTipInfoBar::GetDelegate() {
  return static_cast<SafetyTipInfoBarDelegate*>(delegate());
}

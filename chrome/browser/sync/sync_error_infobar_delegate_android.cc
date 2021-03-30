// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_error_infobar_delegate_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/android/chrome_jni_headers/SyncErrorInfoBar_jni.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/sync_error_infobar.h"
#include "content/public/browser/web_contents.h"

using base::android::JavaParamRef;

SyncErrorInfoBarDelegateAndroid::SyncErrorInfoBarDelegateAndroid() = default;

SyncErrorInfoBarDelegateAndroid::~SyncErrorInfoBarDelegateAndroid() = default;

base::android::ScopedJavaLocalRef<jobject>
SyncErrorInfoBarDelegateAndroid::CreateRenderInfoBar(JNIEnv* env) {
  base::android::ScopedJavaLocalRef<jobject> java_infobar =
      Java_SyncErrorInfoBar_show(env);
  java_delegate_.Reset(java_infobar);
  return java_infobar;
}

infobars::InfoBarDelegate::InfoBarIdentifier
SyncErrorInfoBarDelegateAndroid::GetIdentifier() const {
  return SYNC_ERROR_INFOBAR_DELEGATE_ANDROID;
}

std::u16string SyncErrorInfoBarDelegateAndroid::GetMessageText() const {
  // Message is set in SyncErrorInfoBar.java.
  return std::u16string();
}

bool SyncErrorInfoBarDelegateAndroid::Accept() {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  DCHECK(java_delegate_);
  Java_SyncErrorInfoBar_accept(env, java_delegate_);
  return true;
}

void SyncErrorInfoBarDelegateAndroid::InfoBarDismissed() {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  DCHECK(java_delegate_);
  Java_SyncErrorInfoBar_dismissed(env, java_delegate_);
}

// JNI for SyncErrorInfoBar.
void JNI_SyncErrorInfoBar_Launch(JNIEnv* env,
                                 const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  infobar_service->AddInfoBar(std::make_unique<SyncErrorInfoBar>(
      base::WrapUnique(new SyncErrorInfoBarDelegateAndroid())));
}

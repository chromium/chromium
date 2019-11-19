// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_reduction_proxy/data_reduction_promo_infobar_delegate_android.h"

#include "chrome/android/chrome_jni_headers/DataReductionPromoInfoBarDelegate_jni.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

// static
void DataReductionPromoInfoBarDelegateAndroid::Create(
    content::WebContents* web_contents) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  infobar_service->AddInfoBar(
      DataReductionPromoInfoBarDelegateAndroid::CreateInfoBar(
          infobar_service,
          std::make_unique<DataReductionPromoInfoBarDelegateAndroid>()));
}

DataReductionPromoInfoBarDelegateAndroid::
    DataReductionPromoInfoBarDelegateAndroid() {}

DataReductionPromoInfoBarDelegateAndroid::
    ~DataReductionPromoInfoBarDelegateAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DataReductionPromoInfoBarDelegate_onNativeDestroyed(env);
}

// static
void DataReductionPromoInfoBarDelegateAndroid::Launch(
    JNIEnv* env,
    const JavaRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  Create(web_contents);
}

base::android::ScopedJavaLocalRef<jobject>
DataReductionPromoInfoBarDelegateAndroid::CreateRenderInfoBar(JNIEnv* env) {
  return Java_DataReductionPromoInfoBarDelegate_showPromoInfoBar(env);
}

infobars::InfoBarDelegate::InfoBarIdentifier
DataReductionPromoInfoBarDelegateAndroid::GetIdentifier() const {
  return DATA_REDUCTION_PROMO_INFOBAR_DELEGATE_ANDROID;
}

base::string16 DataReductionPromoInfoBarDelegateAndroid::GetMessageText()
    const {
  // Message is set in DataReductionPromoInfoBar.java.
  return base::string16();
}

bool DataReductionPromoInfoBarDelegateAndroid::Accept() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DataReductionPromoInfoBarDelegate_accept(env);
  return true;
}

// JNI for DataReductionPromoInfoBarDelegate.
void JNI_DataReductionPromoInfoBarDelegate_Launch(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  DataReductionPromoInfoBarDelegateAndroid::Launch(env, jweb_contents);
}

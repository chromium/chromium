// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/web_contents.h"
#include "jni/TranslateBridge_jni.h"

static void JNI_TranslateBridge_Translate(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(web_contents);
  DCHECK(chrome_translate_client);
  translate::TranslateManager* manager =
      chrome_translate_client->GetTranslateManager();
  DCHECK(manager);
  manager->InitiateManualTranslation();
}

static jboolean JNI_TranslateBridge_CanManuallyTranslate(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(web_contents);
  DCHECK(chrome_translate_client);
  translate::TranslateManager* manager =
      chrome_translate_client->GetTranslateManager();
  DCHECK(manager);
  return manager->CanManuallyTranslate();
}

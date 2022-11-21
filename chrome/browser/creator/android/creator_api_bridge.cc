// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <string>

#include "url/gurl.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/creator/android/jni_headers/CreatorApiBridge_jni.h"
#include "components/creator/public/creator_api.h"
#include "url/android/gurl_android.h"

namespace creator {

namespace {

base::android::ScopedJavaLocalRef<jobject> ToJava(JNIEnv* env,
                                                  const Creator& creator) {
  return Java_Creator_Constructor(
      env, base::android::ConvertUTF16ToJavaString(env, creator.url),
      base::android::ConvertUTF16ToJavaString(env, creator.title));
}

// TODO(crbug/1374058): Replace this with actual access to creator api stub.
void DoGetCreator(std::string web_channel_id,
                  base::OnceCallback<void(Creator)> callback) {
  std::move(callback).Run(Creator{u"alexainsley.com", u"Alex Ainsley"});
}

}  // namespace

static void JNI_CreatorApiBridge_GetCreator(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_web_channel_id,
    const base::android::JavaParamRef<jobject>& j_callback) {
  DoGetCreator(base::android::ConvertJavaStringToUTF8(env, j_web_channel_id),
               base::BindOnce(
                   [](const base::android::JavaRef<jobject>& j_callback,
                      Creator creator) {
                     JNIEnv* env = base::android::AttachCurrentThread();
                     base::android::RunObjectCallbackAndroid(
                         j_callback, ToJava(env, creator));
                   },
                   base::android::ScopedJavaGlobalRef(j_callback)));
}

}  // namespace creator

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_http_auth_handler.h"

#include <optional>
#include <utility>

#include "android_webview/browser/aw_contents.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/auth.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwHttpAuthHandler_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;
using content::BrowserThread;

namespace android_webview {

AwHttpAuthHandler::AwHttpAuthHandler(const net::AuthChallengeInfo& auth_info,
                                     content::WebContents* web_contents,
                                     bool first_auth_attempt,
                                     LoginAuthRequiredCallback callback)
    : host_(auth_info.challenger.host()),
      realm_(auth_info.realm),
      callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (web_contents) {
    web_contents_ = web_contents->GetWeakPtr();
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  http_auth_handler_.Reset(Java_AwHttpAuthHandler_create(
      env, reinterpret_cast<intptr_t>(this), first_auth_attempt));

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&AwHttpAuthHandler::Start, weak_factory_.GetWeakPtr()));
}

AwHttpAuthHandler::~AwHttpAuthHandler() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Java_AwHttpAuthHandler_handlerDestroyed(base::android::AttachCurrentThread(),
                                          http_auth_handler_);
}

void AwHttpAuthHandler::Proceed(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                const JavaParamRef<jstring>& user,
                                const JavaParamRef<jstring>& password) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (callback_) {
    std::move(callback_).Run(
        net::AuthCredentials(ConvertJavaStringToUTF16(env, user),
                             ConvertJavaStringToUTF16(env, password)));
  }
}

void AwHttpAuthHandler::Cancel(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (callback_) {
    std::move(callback_).Run(std::nullopt);
  }
}

void AwHttpAuthHandler::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The WebContents may have been destroyed during the PostTask. There may also
  // be no WebContents if the http auth request is from a service worker. This
  // feature is currently unsupported by Android Webview.
  if (!web_contents_) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  AwContents* aw_contents = AwContents::FromWebContents(web_contents_.get());
  if (!aw_contents->OnReceivedHttpAuthRequest(http_auth_handler_, host_,
                                              realm_)) {
    std::move(callback_).Run(std::nullopt);
  }
}

}  // namespace android_webview

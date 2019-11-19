// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_HTTP_AUTH_HANDLER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_HTTP_AUTH_HANDLER_H_

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace net {
class AuthChallengeInfo;
}

namespace android_webview {

// Bridges the Java class of the same name and content::LoginDelegate.
class AwHttpAuthHandler : public content::LoginDelegate,
                          public content::WebContentsObserver {
 public:
  AwHttpAuthHandler(const net::AuthChallengeInfo& auth_info,
                    content::WebContents* web_contents,
                    bool first_auth_attempt,
                    LoginAuthRequiredCallback callback);
  ~AwHttpAuthHandler() override;

  // from AwHttpAuthHandler
  bool HandleOnUIThread(content::WebContents* web_contents);

  void Proceed(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               const base::android::JavaParamRef<jstring>& username,
               const base::android::JavaParamRef<jstring>& password);
  void Cancel(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  void Start();

  base::android::ScopedJavaGlobalRef<jobject> http_auth_handler_;
  std::string host_;
  std::string realm_;
  LoginAuthRequiredCallback callback_;
  base::WeakPtrFactory<AwHttpAuthHandler> weak_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_HTTP_AUTH_HANDLER_H_

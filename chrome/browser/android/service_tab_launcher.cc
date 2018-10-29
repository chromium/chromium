// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/service_tab_launcher.h"

#include <memory>

#include "base/android/jni_string.h"
#include "base/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "jni/ServiceTabLauncher_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// Called by Java when the WebContents instance for a request Id is available.
void JNI_ServiceTabLauncher_OnWebContentsForRequestAvailable(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    jint request_id,
    const JavaParamRef<jobject>& android_web_contents) {
  ServiceTabLauncher::GetInstance()->OnTabLaunched(
      request_id,
      content::WebContents::FromJavaWebContents(android_web_contents));
}

// static
ServiceTabLauncher* ServiceTabLauncher::GetInstance() {
  return base::Singleton<ServiceTabLauncher>::get();
}

ServiceTabLauncher::ServiceTabLauncher() {
}

ServiceTabLauncher::~ServiceTabLauncher() {}

void ServiceTabLauncher::LaunchTab(content::BrowserContext* browser_context,
                                   const content::OpenURLParams& params,
                                   const TabLaunchedCallback& callback) {
  WindowOpenDisposition disposition = params.disposition;
  if (disposition != WindowOpenDisposition::NEW_WINDOW &&
      disposition != WindowOpenDisposition::NEW_POPUP &&
      disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
      disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    // ServiceTabLauncher can currently only launch new tabs.
    NOTIMPLEMENTED();
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> url = ConvertUTF8ToJavaString(
      env, params.url.spec());
  ScopedJavaLocalRef<jstring> referrer_url =
      ConvertUTF8ToJavaString(env, params.referrer.url.spec());
  ScopedJavaLocalRef<jstring> headers = ConvertUTF8ToJavaString(
      env, params.extra_headers);

  ScopedJavaLocalRef<jobject> post_data;

  int request_id = tab_launched_callbacks_.Add(
      std::make_unique<TabLaunchedCallback>(callback));
  DCHECK_GE(request_id, 1);

  Java_ServiceTabLauncher_launchTab(
      env, request_id, browser_context->IsOffTheRecord(), url,
      static_cast<int>(disposition), referrer_url,
      static_cast<int>(params.referrer.policy), headers, post_data);
}

void ServiceTabLauncher::OnTabLaunched(int request_id,
                                       content::WebContents* web_contents) {
  TabLaunchedCallback* callback = tab_launched_callbacks_.Lookup(request_id);
  DCHECK(callback);

  if (callback)
    callback->Run(web_contents);

  tab_launched_callbacks_.Remove(request_id);
}

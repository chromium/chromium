// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/policy/policy_auditor_bridge.h"

#include "base/android/jni_android.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PolicyAuditorBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::NavigationHandle;
using content::RenderFrameHost;
using content::WebContents;
using content::WebContentsObserver;
using content::WebContentsUserData;
using url::GURLAndroid;

PolicyAuditorBridge::PolicyAuditorBridge(
    WebContents* web_contents,
    ScopedJavaLocalRef<jobject>&& android_policy_auditor)
    : WebContentsObserver(web_contents),
      WebContentsUserData<PolicyAuditorBridge>(*web_contents),
      android_policy_auditor_(android_policy_auditor) {
  // if android doesn't supply a policy auditor, there is no reason to to create
  // this class
  DCHECK(!android_policy_auditor_.is_null());
}

PolicyAuditorBridge::~PolicyAuditorBridge() = default;

// static
void PolicyAuditorBridge::CreateForWebContents(WebContents* web_contents) {
  ScopedJavaLocalRef<jobject> android_policy_auditor =
      Java_PolicyAuditorBridge_getPolicyAuditor(AttachCurrentThread());

  if (!android_policy_auditor.is_null()) {
    WebContentsUserData<PolicyAuditorBridge>::CreateForWebContents(
        web_contents, std::move(android_policy_auditor));
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PolicyAuditorBridge);

void PolicyAuditorBridge::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    Java_PolicyAuditorBridge_notifyAuditEventForDidFinishNavigation(
        AttachCurrentThread(), navigation_handle->GetJavaNavigationHandle(),
        android_policy_auditor_);
  }
}

void PolicyAuditorBridge::DidFinishLoad(RenderFrameHost* render_frame_host,
                                        const GURL& validated_url) {
  if (render_frame_host->IsInPrimaryMainFrame()) {
    JNIEnv* env = AttachCurrentThread();
    Java_PolicyAuditorBridge_notifyAuditEventForDidFinishLoad(
        env, GURLAndroid::FromNativeGURL(env, validated_url),
        android_policy_auditor_);
  }
}

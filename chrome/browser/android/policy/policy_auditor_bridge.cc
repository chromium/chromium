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

namespace {

bool HasPolicyAuditor(JNIEnv* env) {
  return !Java_PolicyAuditorBridge_maybeGetPolicyAuditorInstance(env).is_null();
}

}  // namespace

PolicyAuditorBridge::PolicyAuditorBridge(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<PolicyAuditorBridge>(*web_contents) {}

PolicyAuditorBridge::~PolicyAuditorBridge() = default;

// static
void PolicyAuditorBridge::CreateForWebContents(WebContents* web_contents) {
  if (HasPolicyAuditor(AttachCurrentThread())) {
    WebContentsUserData<PolicyAuditorBridge>::CreateForWebContents(
        web_contents);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PolicyAuditorBridge);

void PolicyAuditorBridge::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    JNIEnv* env = AttachCurrentThread();
    DCHECK(HasPolicyAuditor(env));
    Java_PolicyAuditorBridge_notifyAuditEventForDidFinishNavigation(
        env, navigation_handle->GetJavaNavigationHandle());
  }
}

void PolicyAuditorBridge::DidFinishLoad(RenderFrameHost* render_frame_host,
                                        const GURL& validated_url) {
  if (render_frame_host->IsInPrimaryMainFrame()) {
    JNIEnv* env = AttachCurrentThread();
    DCHECK(HasPolicyAuditor(env));
    Java_PolicyAuditorBridge_notifyAuditEventForDidFinishLoad(
        env, GURLAndroid::FromNativeGURL(env, validated_url));
  }
}

DEFINE_JNI(PolicyAuditorBridge)

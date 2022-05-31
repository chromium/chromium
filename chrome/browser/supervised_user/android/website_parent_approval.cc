// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/android/website_parent_approval.h"

#include <jni.h>
#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/no_destructor.h"
#include "chrome/browser/supervised_user/jni_headers/WebsiteParentApproval_jni.h"
#include "chrome/browser/supervised_user/web_approvals_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

// Stores the callback passed in to an ongoing RequestLocalApproval call.
// We can only have a single local approval in progress at a time on Android
// as the implementation is a bottom sheet (which is dismissed if it loses
// focus).
base::OnceCallback<void(bool)>* GetOnCompletionCallback() {
  static base::NoDestructor<base::OnceCallback<void(bool)>> callback;
  return callback.get();
}

// static
bool WebsiteParentApproval::IsLocalApprovalSupported() {
  return Java_WebsiteParentApproval_isLocalApprovalSupported(
      base::android::AttachCurrentThread());
}

void WebsiteParentApproval::RequestLocalApproval(
    content::WebContents* web_contents,
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  ui::WindowAndroid* window_android =
      web_contents->GetNativeView()->GetWindowAndroid();

  *GetOnCompletionCallback() = std::move(callback);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebsiteParentApproval_requestLocalApproval(
      env, window_android->GetJavaObject(),
      url::GURLAndroid::FromNativeGURL(env, url));
}

void JNI_WebsiteParentApproval_OnCompletion(JNIEnv* env, jboolean jboolean) {
  // Check that we have a callback stored from the local approval request and
  // call it.
  auto* cb = GetOnCompletionCallback();
  DCHECK(cb != nullptr);
  std::move(*cb).Run(jboolean);
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/android/website_parent_approval.h"

#include <jni.h>

#include <memory>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/android/favicon_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/supervised_user/website_parent_approval_jni_headers/WebsiteParentApproval_jni.h"

using base::android::JavaParamRef;

// Stores the callback passed in to an ongoing RequestLocalApproval call.
// We can only have a single local approval in progress at a time on Android
// as the implementation is a bottom sheet (which is dismissed if it loses
// focus).
base::OnceCallback<void(AndroidLocalWebApprovalFlowOutcome)>*
GetOnCompletionCallback() {
  static base::NoDestructor<
      base::OnceCallback<void(AndroidLocalWebApprovalFlowOutcome)>>
      callback;
  return callback.get();
}

// static
bool WebsiteParentApproval::IsLocalApprovalSupported() {
  return Java_WebsiteParentApproval_isLocalApprovalSupported(
      base::android::AttachCurrentThread());
}

// static
void WebsiteParentApproval::RequestLocalApproval(
    content::WebContents* web_contents,
    const GURL& url,
    base::OnceCallback<void(AndroidLocalWebApprovalFlowOutcome)> callback,
    Profile& profile) {
  if (!GetOnCompletionCallback()->is_null()) {
    // There is a pending operation in progress. This is
    // possible if for example the user clicks the request approval button in
    // quick succession before the auth bottom sheet is displayed.
    // Recover by just dropping the second operation.
    std::move(callback).Run(AndroidLocalWebApprovalFlowOutcome::kIncomplete);
    return;
  }

  ui::WindowAndroid* window_android =
      web_contents->GetNativeView()->GetWindowAndroid();

  *GetOnCompletionCallback() = std::move(callback);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebsiteParentApproval_requestLocalApproval(
      env, window_android->GetJavaObject(),
      url::GURLAndroid::FromNativeGURL(env, url), profile.GetJavaObject());
}

void JNI_WebsiteParentApproval_OnCompletion(JNIEnv* env,
                                            jint flow_outcome_value) {
  // Check that we have a callback stored from the local approval request and
  // call it.
  auto* cb = GetOnCompletionCallback();
  DCHECK(!cb->is_null());
  AndroidLocalWebApprovalFlowOutcome flow_outcome_enum =
      static_cast<AndroidLocalWebApprovalFlowOutcome>(flow_outcome_value);
  std::move(*cb).Run(flow_outcome_enum);
}

// Triggers the asynchronous favicon request for a provided url.
// Returns it via the provided callback.
static void JNI_WebsiteParentApproval_FetchFavicon(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_url,
    jint min_source_size_in_pixel,
    jint desired_size_in_pixel,
    const base::android::JavaParamRef<jobject>& on_favicon_fetched_callback) {
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);

  FaviconFetcher* faviconFetcher =
      new FaviconFetcher(LargeIconServiceFactory::GetForBrowserContext(
          ProfileManager::GetActiveUserProfile()));

  faviconFetcher->FetchFavicon(
      url, true, min_source_size_in_pixel, desired_size_in_pixel,
      base::android::ScopedJavaGlobalRef<jobject>(on_favicon_fetched_callback));
}

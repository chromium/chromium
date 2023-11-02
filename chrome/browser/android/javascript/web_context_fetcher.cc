// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/WebContextFetcher_jni.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;

static void OnContextFetchComplete(
    const ScopedJavaGlobalRef<jobject>& scoped_jcallback,
    base::TimeTicks javascript_start,
    base::Value result) {
  if (!javascript_start.is_null()) {
    base::TimeDelta javascript_time = base::TimeTicks::Now() - javascript_start;
    // TODO(benwgold): Update so that different js scripts can monitor execution
    // separately.
    base::UmaHistogramTimes("WebContextFetcher.JavaScriptRunner.ExecutionTime",
                            javascript_time);
    DVLOG(1) << "WebContextFetcher.JavaScriptRunner.ExecutionTime = "
             << javascript_time;
  }
  base::android::RunStringCallbackAndroid(scoped_jcallback, result.GetString());
}

// IMPORTANT: The output of this fetch should only be handled in memory safe
//      languages (Java) and should not be parsed in C++.
static void ExecuteFetch(const std::u16string& script,
                         const ScopedJavaGlobalRef<jobject>& scoped_jcallback,
                         content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);
  // TODO(benwgold): Consider adding handling for cases when the document is not
  // yet ready.
  base::OnceCallback<void(base::Value)> callback = base::BindOnce(
      &OnContextFetchComplete, scoped_jcallback, base::TimeTicks::Now());
  render_frame_host->ExecuteJavaScriptInIsolatedWorld(
      script, std::move(callback), ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

static void JNI_WebContextFetcher_FetchContextWithJavascript(
    JNIEnv* env,
    const JavaParamRef<jstring>& jscript,
    const JavaParamRef<jobject>& jcallback,
    const JavaParamRef<jobject>& jrender_frame_host) {
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jrender_frame_host);
  std::u16string script = base::android::ConvertJavaStringToUTF16(env, jscript);
  ScopedJavaGlobalRef<jobject> scoped_jcallback(env, jcallback);
  ExecuteFetch(script, scoped_jcallback, render_frame_host);
}

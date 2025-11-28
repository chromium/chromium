// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/tab_utils.h"

#include <string>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/DomDistillerTabUtils_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace android {

static void JNI_DomDistillerTabUtils_DistillCurrentPageAndViewIfSuccessful(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_web_contents,
    const JavaParamRef<jobject>& j_callback) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ::DistillCurrentPageAndViewIfSuccessful(
      web_contents,
      base::BindOnce(
          [](const jni_zero::ScopedJavaGlobalRef<jobject>& callback,
             bool success) {
            if (callback) {
              base::android::RunBooleanCallbackAndroid(callback, success);
            }
          },
          jni_zero::ScopedJavaGlobalRef<jobject>(j_callback)));
}

static void JNI_DomDistillerTabUtils_DistillCurrentPage(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_source_web_contents) {
  content::WebContents* source_web_contents =
      content::WebContents::FromJavaWebContents(j_source_web_contents);
  ::DistillCurrentPage(source_web_contents);
}

static void JNI_DomDistillerTabUtils_DistillAndView(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_source_web_contents,
    const JavaParamRef<jobject>& j_destination_web_contents) {
  content::WebContents* source_web_contents =
      content::WebContents::FromJavaWebContents(j_source_web_contents);
  content::WebContents* destination_web_contents =
      content::WebContents::FromJavaWebContents(j_destination_web_contents);
  ::DistillAndView(source_web_contents, destination_web_contents);
}

static std::u16string
JNI_DomDistillerTabUtils_GetFormattedUrlFromOriginalDistillerUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_url) {
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);

  if (url.spec().length() > content::kMaxURLDisplayChars)
    url = url.IsStandard() ? url.DeprecatedGetOriginAsURL()
                           : GURL(url.GetScheme() + ":");

  // Note that we can't unescape spaces here, because if the user copies this
  // and pastes it into another program, that program may think the URL ends at
  // the space.
  return url_formatter::FormatUrl(url, url_formatter::kFormatUrlOmitDefaults,
                                  base::UnescapeRule::NORMAL, nullptr, nullptr,
                                  nullptr);
}

static jint JNI_DomDistillerTabUtils_GetDistillerHeuristics(JNIEnv* env) {
  return static_cast<jint>(dom_distiller::GetDistillerHeuristicsType());
}

static void JNI_DomDistillerTabUtils_SetInterceptNavigationDelegate(
    JNIEnv* env,
    const JavaParamRef<jobject>& delegate,
    const JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  DCHECK(web_contents);
  navigation_interception::InterceptNavigationDelegate::Associate(
      web_contents,
      std::make_unique<navigation_interception::InterceptNavigationDelegate>(
          env, delegate));
}

static void JNI_DomDistillerTabUtils_RunReadabilityHeuristicsOnWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_web_contents,
    const JavaParamRef<jobject>& j_callback) {
  base::OnceCallback<void(bool)> callback =
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback));
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ::RunReadabilityHeuristicsOnWebContents(web_contents, std::move(callback));
}

}  // namespace android

DEFINE_JNI(DomDistillerTabUtils)

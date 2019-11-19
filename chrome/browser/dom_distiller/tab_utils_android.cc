// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/DomDistillerTabUtils_jni.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace android {

void JNI_DomDistillerTabUtils_DistillCurrentPageAndView(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ::DistillCurrentPageAndView(web_contents);
}

void JNI_DomDistillerTabUtils_DistillCurrentPage(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_source_web_contents) {
  content::WebContents* source_web_contents =
      content::WebContents::FromJavaWebContents(j_source_web_contents);
  ::DistillCurrentPage(source_web_contents);
}

void JNI_DomDistillerTabUtils_DistillAndView(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_source_web_contents,
    const JavaParamRef<jobject>& j_destination_web_contents) {
  content::WebContents* source_web_contents =
      content::WebContents::FromJavaWebContents(j_source_web_contents);
  content::WebContents* destination_web_contents =
      content::WebContents::FromJavaWebContents(j_destination_web_contents);
  ::DistillAndView(source_web_contents, destination_web_contents);
}

ScopedJavaLocalRef<jstring>
JNI_DomDistillerTabUtils_GetFormattedUrlFromOriginalDistillerUrl(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_url) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));

  if (url.spec().length() > content::kMaxURLDisplayChars)
    url = url.IsStandard() ? url.GetOrigin() : GURL(url.scheme() + ":");

  // Note that we can't unescape spaces here, because if the user copies this
  // and pastes it into another program, that program may think the URL ends at
  // the space.
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrl(url, url_formatter::kFormatUrlOmitDefaults,
                                    net::UnescapeRule::NORMAL, nullptr, nullptr,
                                    nullptr));
}

jint JNI_DomDistillerTabUtils_GetDistillerHeuristics(JNIEnv* env) {
  return static_cast<jint>(dom_distiller::GetDistillerHeuristicsType());
}

void JNI_DomDistillerTabUtils_SetInterceptNavigationDelegate(
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

}  // namespace android

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/trusted_cdn.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/tab/jni_headers/TrustedCdn_jni.h"
#include "components/embedder_support/android/util/cdn_utils.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

TrustedCdn::TrustedCdn(JNIEnv* env, const JavaParamRef<jobject>& obj)
    : jobj_(env, obj) {}

TrustedCdn::~TrustedCdn() = default;

void TrustedCdn::SetWebContents(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                const JavaParamRef<jobject>& jweb_contents) {
  WebContentsObserver::Observe(WebContents::FromJavaWebContents(jweb_contents));
}

void TrustedCdn::ResetWebContents(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj) {
  WebContentsObserver::Observe(nullptr);
}

void TrustedCdn::OnDestroyed(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

// TrustedCdn should only track primary pages and should skip subframe,
// same-document, or non-committed navigations (downloads or 204/205 responses).
void TrustedCdn::PrimaryPageChanged(content::Page& page) {
  GURL publisher_url;

  // Offline pages don't have headers when they are loaded.
  // TODO(bauerb): Consider storing the publisher URL on the offline page item.
  if (!offline_pages::OfflinePageUtils::GetOfflinePageFromWebContents(
          WebContents::FromRenderFrameHost(&page.GetMainDocument()))) {
    publisher_url = embedder_support::GetPublisherURL(page);
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_publisher_url;
  if (publisher_url.is_valid())
    j_publisher_url = ConvertUTF8ToJavaString(env, publisher_url.spec());

  Java_TrustedCdn_setPublisherUrl(env, jobj_, j_publisher_url);
}

static jlong JNI_TrustedCdn_Init(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new TrustedCdn(env, obj));
}

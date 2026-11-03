// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "components/embedder_support/android/util/cdn_utils.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab/jni_headers/TrustedCdn_jni.h"

static GURL JNI_TrustedCdn_GetPublisherUrl(JNIEnv* env,
                                           content::WebContents* web_contents) {
  if (!web_contents || web_contents->IsBeingDestroyed()) {
    return GURL();
  }

  if (offline_pages::OfflinePageUtils::GetOfflinePageFromWebContents(
          web_contents)) {
    return GURL();
  }

  return embedder_support::GetPublisherURL(web_contents->GetPrimaryMainFrame());
}

DEFINE_JNI(TrustedCdn)

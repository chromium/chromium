// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/trusted_cdn.h"

#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "components/embedder_support/android/util/cdn_utils.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab/jni_headers/TrustedCdn_jni.h"

using content::WebContents;

TrustedCdn::TrustedCdn() = default;

TrustedCdn::~TrustedCdn() = default;

void TrustedCdn::SetWebContents(content::WebContents* web_contents) {
  web_contents_ = web_contents;
}

void TrustedCdn::ResetWebContents() {
  web_contents_ = nullptr;
}

void TrustedCdn::OnDestroyed() {
  delete this;
}

GURL TrustedCdn::GetPublisherUrl() {
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    return GURL();
  }

  if (offline_pages::OfflinePageUtils::GetOfflinePageFromWebContents(
          web_contents_)) {
    return GURL();
  }

  return embedder_support::GetPublisherURL(
      web_contents_->GetPrimaryMainFrame());
}

static int64_t JNI_TrustedCdn_Init() {
  return reinterpret_cast<intptr_t>(new TrustedCdn());
}

DEFINE_JNI(TrustedCdn)

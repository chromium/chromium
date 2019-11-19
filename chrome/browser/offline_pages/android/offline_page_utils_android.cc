// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_utils.h"

#include "base/logging.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/offline_pages/android/downloads/offline_page_download_bridge.h"
#include "chrome/browser/offline_pages/android/downloads/offline_page_infobar_delegate.h"
#include "content/public/browser/web_contents.h"

// Android-specific part of OfflinePageUtils.
// TODO(dimich): consider callsites to generalize.

namespace offline_pages {

// static
bool OfflinePageUtils::GetTabId(content::WebContents* web_contents,
                                int* tab_id) {
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  if (!tab_android)
    return false;
  *tab_id = tab_android->GetAndroidId();
  return true;
}

// static
bool OfflinePageUtils::CurrentlyShownInCustomTab(
    content::WebContents* web_contents) {
  auto* delegate = static_cast<::android::TabWebContentsDelegateAndroid*>(
      web_contents->GetDelegate());
  return delegate && delegate->IsCustomTab();
}

// static
void OfflinePageUtils::ShowDuplicatePrompt(
    const base::Closure& confirm_continuation,
    const GURL& url,
    bool exists_duplicate_request,
    content::WebContents* web_contents) {
  OfflinePageInfoBarDelegate::Create(
      confirm_continuation, url, exists_duplicate_request, web_contents);
}

// static
void OfflinePageUtils::ShowDownloadingToast() {
  android::OfflinePageDownloadBridge::ShowDownloadingToast();
}

}  // namespace offline_pages

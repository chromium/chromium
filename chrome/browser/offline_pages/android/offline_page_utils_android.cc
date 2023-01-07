// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_utils.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/download/android/download_dialog_utils.h"
#include "chrome/browser/download/android/duplicate_download_dialog_bridge.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/offline_pages/android/downloads/offline_page_download_bridge.h"
#include "chrome/browser/offline_pages/android/downloads/offline_page_infobar_delegate.h"
#include "content/public/browser/web_contents.h"

// Android-specific part of OfflinePageUtils.
// TODO(dimich): consider callsites to generalize.

namespace offline_pages {

namespace {

void OnDuplicateDialogConfirmed(base::OnceClosure callback, bool accepted) {
  if (accepted)
    std::move(callback).Run();
}

}  // namespace

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
    base::OnceClosure confirm_continuation,
    const GURL& url,
    bool exists_duplicate_request,
    content::WebContents* web_contents) {
  DuplicateDownloadDialogBridge::GetInstance()->Show(
      url.spec(), DownloadDialogUtils::GetDisplayURLForPageURL(url),
      -1 /*total_bytes*/, exists_duplicate_request, web_contents,
      base::BindOnce(&OnDuplicateDialogConfirmed,
                     std::move(confirm_continuation)));
}

// static
void OfflinePageUtils::ShowDownloadingToast() {
  android::OfflinePageDownloadBridge::ShowDownloadingToast();
}

}  // namespace offline_pages

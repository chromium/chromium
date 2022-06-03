// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nfc/chrome_nfc_permission_context_delegate.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#endif

ChromeNfcPermissionContextDelegate::ChromeNfcPermissionContextDelegate() =
    default;

ChromeNfcPermissionContextDelegate::~ChromeNfcPermissionContextDelegate() =
    default;

#if defined(OS_ANDROID)
bool ChromeNfcPermissionContextDelegate::IsInteractable(
    content::WebContents* web_contents) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  return tab && tab->IsUserInteractable();
}
#endif

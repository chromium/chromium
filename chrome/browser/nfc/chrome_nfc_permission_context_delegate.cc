// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nfc/chrome_nfc_permission_context_delegate.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#endif

ChromeNfcPermissionContextDelegate::ChromeNfcPermissionContextDelegate() =
    default;

ChromeNfcPermissionContextDelegate::~ChromeNfcPermissionContextDelegate() =
    default;

#if BUILDFLAG(IS_ANDROID)
bool ChromeNfcPermissionContextDelegate::IsInteractable(
    content::WebContents* web_contents) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  return tab && tab->IsUserInteractable();
}
#endif

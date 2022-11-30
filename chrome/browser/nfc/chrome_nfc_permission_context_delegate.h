// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NFC_CHROME_NFC_PERMISSION_CONTEXT_DELEGATE_H_
#define CHROME_BROWSER_NFC_CHROME_NFC_PERMISSION_CONTEXT_DELEGATE_H_

#include "build/build_config.h"
#include "components/permissions/contexts/nfc_permission_context.h"

class ChromeNfcPermissionContextDelegate
    : public permissions::NfcPermissionContext::Delegate {
 public:
  ChromeNfcPermissionContextDelegate();

  ChromeNfcPermissionContextDelegate(
      const ChromeNfcPermissionContextDelegate&) = delete;
  ChromeNfcPermissionContextDelegate& operator=(
      const ChromeNfcPermissionContextDelegate&) = delete;

  ~ChromeNfcPermissionContextDelegate() override;

  // NfcPermissionContext::Delegate:
#if BUILDFLAG(IS_ANDROID)
  bool IsInteractable(content::WebContents* web_contents) override;
#endif
};

#endif  // CHROME_BROWSER_NFC_CHROME_NFC_PERMISSION_CONTEXT_DELEGATE_H_

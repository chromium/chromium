// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_HELPER_BRIDGE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_HELPER_BRIDGE_IMPL_H_

#include <jni.h>

#include "chrome/browser/password_manager/android/password_manager_error_message_helper_bridge.h"
#include "content/public/browser/web_contents.h"

class PasswordManagerErrorMessageHelperBridgeImpl
    : public PasswordManagerErrorMessageHelperBridge {
 public:
  void StartUpdateAccountCredentialsFlow(
      content::WebContents* web_contents) override;
  void StartTrustedVaultKeyRetrievalFlow(
      content::WebContents* web_contents) override;
  bool ShouldShowSignInErrorUI(content::WebContents* web_contents) override;
  bool ShouldShowUpdateGMSCoreErrorUI(
      content::WebContents* web_contents) override;
  void SaveErrorUIShownTimestamp(content::WebContents* web_contents) override;
  void LaunchGmsUpdate(content::WebContents* web_contents) override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_HELPER_BRIDGE_IMPL_H_

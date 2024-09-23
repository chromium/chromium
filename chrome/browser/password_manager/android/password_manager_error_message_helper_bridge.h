// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_HELPER_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_HELPER_BRIDGE_H_

#include <jni.h>

#include "content/public/browser/web_contents.h"

class PasswordManagerErrorMessageHelperBridge {
 public:
  virtual ~PasswordManagerErrorMessageHelperBridge();

  // An implementation of this method should call the Java method that starts
  // the Android process to update credentials for the primary account in
  // Chrome. This method will only work for users that have been previously
  // signed in Chrome on the device.
  virtual void StartUpdateAccountCredentialsFlow(
      content::WebContents* web_contents) = 0;

  // An implementation of this method should call the Java method that starts
  // the Android process to retrieve key for on-device encryption. This method
  // will only work for users that are currently syncing.
  virtual void StartTrustedVaultKeyRetrievalFlow(
      content::WebContents* web_contents) = 0;

  // Checks if enough time has passed since the last error UI was shown.
  virtual bool ShouldShowSignInErrorUI(content::WebContents* web_contents) = 0;
  virtual bool ShouldShowUpdateGMSCoreErrorUI(
      content::WebContents* web_contents) = 0;

  // Saves the timestam at which the error UI was shown.
  virtual void SaveErrorUIShownTimestamp(
      content::WebContents* web_contents) = 0;

  // Starts the Google Play services page where the user can choose to update
  // GMSCore.
  virtual void LaunchGmsUpdate(content::WebContents* web_contents) = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_HELPER_BRIDGE_H_

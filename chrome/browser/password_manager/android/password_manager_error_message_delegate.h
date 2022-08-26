// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_DELEGATE_H_

#include "components/messages/android/message_wrapper.h"

namespace content {
class WebContents;
}  // namespace content

class PasswordManagerErrorMessageDelegate {
 public:
  PasswordManagerErrorMessageDelegate();
  ~PasswordManagerErrorMessageDelegate();

  // Displays a password error message for current `web_contents`.
  // `save_password` decides whether the error message mentions the inability to
  // save or use passwords.
  void DisplayPasswordManagerErrorMessage(content::WebContents* web_contents,
                                          bool save_password);
  void DismissPasswordManagerErrorMessage(
      messages::DismissReason dismiss_reason);

  std::unique_ptr<messages::MessageWrapper> message_;

 private:
  void CreateMessage(bool save_password);

  // Following methods handle events associated with user interaction with UI.
  void HandleSignInButtonClicked();
  void HandleMessageDismissed(messages::DismissReason dismiss_reason);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_DELEGATE_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_DELEGATE_H_

#include <memory>

#include "chrome/browser/password_manager/android/password_manager_error_message_helper_bridge.h"
#include "components/messages/android/message_wrapper.h"

namespace content {
class WebContents;
}  // namespace content

class PasswordManagerErrorMessageDelegate {
 public:
  explicit PasswordManagerErrorMessageDelegate(
      std::unique_ptr<PasswordManagerErrorMessageHelperBridge> bridge_);
  ~PasswordManagerErrorMessageDelegate();

  // Displays a password error message for current `web_contents` if enough
  // time has passed since the last error message was displayed.
  // `save_password` decides whether the error message mentions the inability to
  // save or use passwords.
  void MaybeDisplayErrorMessage(content::WebContents* web_contents,
                                bool save_password);
  void DismissPasswordManagerErrorMessage(
      messages::DismissReason dismiss_reason);

 private:
  friend class PasswordManagerErrorMessageDelegateTest;

  std::unique_ptr<messages::MessageWrapper> message_;
  std::unique_ptr<PasswordManagerErrorMessageHelperBridge> helper_bridge_;

  void CreateMessage(content::WebContents* web_contents, bool save_password);

  // Following methods handle events associated with user interaction with UI.
  void HandleSignInButtonClicked(content::WebContents* web_contents);
  void HandleMessageDismissed(messages::DismissReason dismiss_reason);

  void RecordDismissalReasonMetrics(messages::DismissReason dismiss_reason);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_DELEGATE_H_

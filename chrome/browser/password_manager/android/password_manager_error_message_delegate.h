// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_DELEGATE_H_

#include <memory>

#include "chrome/browser/password_manager/android/password_manager_error_message_helper_bridge.h"
#include "components/messages/android/message_wrapper.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/prefs/pref_service.h"

namespace content {
class WebContents;
}  // namespace content

class PasswordManagerErrorMessageDelegate {
 public:
  explicit PasswordManagerErrorMessageDelegate(
      std::unique_ptr<PasswordManagerErrorMessageHelperBridge> bridge_);
  ~PasswordManagerErrorMessageDelegate();

  // Dismisses the message if one is enqueued. Called when the `WebContents` is
  // destroyed, but before the owner of `this` is destroyed. This is to ensure
  // that any message that is still showing is dismissed before the `message_`
  // is destroyed.
  void DismissPasswordManagerErrorMessage(
      messages::DismissReason dismiss_reason);

  // Displays a password error message for current `web_contents` if enough
  // time has passed since the last error message was displayed.
  // `ErrorMessageFlowType` decides whether the error message mentions the
  // inability to save or use passwords. The `pref_service` is used to count
  // how many times the prompt was shown.
  void MaybeDisplayErrorMessage(
      content::WebContents* web_contents,
      PrefService* pref_service,
      password_manager::ErrorMessageFlowType flow_type,
      password_manager::PasswordStoreBackendErrorType error_type,
      base::OnceCallback<void()> dismissal_callback);

 private:
  friend class PasswordManagerErrorMessageDelegateTest;

  bool ShouldShowErrorUI(
      content::WebContents* web_contents,
      password_manager::PasswordStoreBackendErrorType error_type);

  std::unique_ptr<messages::MessageWrapper> CreateMessage(
      content::WebContents* web_contents,
      password_manager::PasswordStoreBackendErrorType error_type,
      base::OnceCallback<void()> dismissal_callback);

  // Following methods handle events associated with user interaction with UI.
  void HandleActionButtonClicked(
      content::WebContents* web_contents,
      password_manager::PasswordStoreBackendErrorType error);
  void HandleMessageDismissed(messages::DismissReason dismiss_reason);

  std::unique_ptr<messages::MessageWrapper> message_;
  std::unique_ptr<PasswordManagerErrorMessageHelperBridge> helper_bridge_;
  password_manager::PasswordStoreBackendErrorType error_type_ =
      password_manager::PasswordStoreBackendErrorType::kUncategorized;

  base::WeakPtrFactory<PasswordManagerErrorMessageDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ERROR_MESSAGE_DELEGATE_H_

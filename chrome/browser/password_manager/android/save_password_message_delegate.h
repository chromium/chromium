// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SAVE_PASSWORD_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SAVE_PASSWORD_MESSAGE_DELEGATE_H_

#include <memory>

#include "components/messages/android/message_wrapper.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

namespace content {
class WebContents;
}  // namespace content

// This class provides simplified interface for ChromePasswordManagerClient to
// display a prompt to save password through Messages API. The class is
// responsible for populating message properties, managing message's lifetime,
// saving password form in response to user interactions and recording metrics.
class SavePasswordMessageDelegate {
 public:
  SavePasswordMessageDelegate();
  ~SavePasswordMessageDelegate();

  // Displays a "Save password" message for current |web_contents| and
  // |form_to_save|.
  void DisplaySavePasswordPrompt(
      content::WebContents* web_contents,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save);
  // Dismisses currently displayed message.
  void DismissSavePasswordPrompt();

 private:
  friend class SavePasswordMessageDelegateTest;

  void CreateMessage(
      content::WebContents* web_contents,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool is_saving_google_account);

  // Called in response to user clicking "Save" and "Never" buttons.
  void HandleSaveClick();
  void HandleNeverClick();
  // Called when the message is dismissed.
  void HandleDismissCallback();

  void RecordMessageShownMetrics();
  void RecordDismissalReasonMetrics();

  content::WebContents* web_contents_ = nullptr;
  std::unique_ptr<messages::MessageWrapper> message_;
  // The PasswordFormManager managing the form we're asking the user about,
  // and should update as per their decision.
  std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save_;
  // Tracks the reason the message was dismissed.
  password_manager::metrics_util::UIDismissalReason ui_dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SAVE_PASSWORD_MESSAGE_DELEGATE_H_

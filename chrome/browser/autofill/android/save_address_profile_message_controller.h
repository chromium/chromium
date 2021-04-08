// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_MESSAGE_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_MESSAGE_CONTROLLER_H_

#include <memory>

#include "base/callback.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// Android implementation of the prompt for saving an address profile
// which uses Messages API. The class is responsible for populating
// message properties, managing message's lifetime and handling user
// interactions.
class SaveAddressProfileMessageController {
 public:
  SaveAddressProfileMessageController();
  SaveAddressProfileMessageController(
      const SaveAddressProfileMessageController&) = delete;
  SaveAddressProfileMessageController& operator=(
      const SaveAddressProfileMessageController&) = delete;
  ~SaveAddressProfileMessageController();

  using PrimaryActionCallback = base::OnceCallback<void(
      content::WebContents*,
      const AutofillProfile&,
      AutofillClient::AddressProfileSavePromptCallback)>;

  void DisplayMessage(content::WebContents* web_contents,
                      const AutofillProfile& profile,
                      AutofillClient::AddressProfileSavePromptCallback
                          save_address_profile_callback,
                      PrimaryActionCallback primary_action_callback);

  // Called in response to user clicking the primary button.
  void OnPrimaryAction();
  // Called whenever the message is dismissed (e.g. after timeout or because the
  // user already accepted or declined the message).
  void OnMessageDismissed(messages::DismissReason dismiss_reason);

 private:
  friend class SaveAddressProfileMessageControllerTest;

  void DismissMessage();

  void RunSaveAddressProfileCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision decision);

  content::WebContents* web_contents_ = nullptr;
  std::unique_ptr<messages::MessageWrapper> message_;

  // The profile which is being confirmed by the user.
  AutofillProfile profile_;
  // The callback to run once the user makes the final decision.
  AutofillClient::AddressProfileSavePromptCallback
      save_address_profile_callback_;
  PrimaryActionCallback primary_action_callback_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_MESSAGE_CONTROLLER_H_

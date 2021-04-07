// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_MESSAGE_DELEGATE_H_

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
class SaveAddressProfileMessageDelegate {
 public:
  SaveAddressProfileMessageDelegate();
  SaveAddressProfileMessageDelegate(const SaveAddressProfileMessageDelegate&) =
      delete;
  SaveAddressProfileMessageDelegate& operator=(
      const SaveAddressProfileMessageDelegate&) = delete;
  ~SaveAddressProfileMessageDelegate();

  void DisplaySavePrompt(
      content::WebContents* web_contents,
      const AutofillProfile& profile,
      AutofillClient::AddressProfileSavePromptCallback callback);

  // Called in response to user clicking "Save".
  void OnSaveClicked();
  // Called when the message is dismissed.
  void OnMessageDismissed(messages::DismissReason dismiss_reason);

 private:
  friend class SaveAddressProfileMessageDelegateTest;

  void DismissSavePrompt();

  void RunCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision decision);

  content::WebContents* web_contents_ = nullptr;
  std::unique_ptr<messages::MessageWrapper> message_;

  // The profile that will be saved if the user accepts.
  AutofillProfile profile_;
  // The callback to run once the user makes a decision.
  AutofillClient::AddressProfileSavePromptCallback callback_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_MESSAGE_DELEGATE_H_

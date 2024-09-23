// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_MESSAGE_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_MESSAGE_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// Android implementation of the prompt for saving new/updating existing address
// profile which uses Messages API. The class is responsible for populating
// message properties, managing message's lifetime and handling user
// interactions.
class SaveUpdateAddressProfileMessageController {
 public:
  // Maximum number of lines this message's description can occupy.
  static constexpr int kDescriptionMaxLines = 2;

  SaveUpdateAddressProfileMessageController();
  SaveUpdateAddressProfileMessageController(
      const SaveUpdateAddressProfileMessageController&) = delete;
  SaveUpdateAddressProfileMessageController& operator=(
      const SaveUpdateAddressProfileMessageController&) = delete;
  ~SaveUpdateAddressProfileMessageController();

  using PrimaryActionCallback = base::OnceCallback<void(
      content::WebContents*,
      const AutofillProfile&,
      const AutofillProfile* original_profile,
      bool is_migration_to_account,
      AutofillClient::AddressProfileSavePromptCallback)>;

  // Triggers a message for saving the `profile` using the given
  // `web_contents`. If another message is already shown, it will be replaced
  // with the incoming one. The `original_profile` is nullptr for a new address
  // or points to the existing profile which is to be updated. User will be
  // offered to migrate their address profile to their Google Account when
  // `is_migration_to_account` is true. `primary_action_callback` is triggered
  // when the user accepts the message, otherwise
  // `save_address_profile_callback` is run with the corresponding decision.
  void DisplayMessage(content::WebContents* web_contents,
                      const AutofillProfile& profile,
                      const AutofillProfile* original_profile,
                      bool is_migration_to_account,
                      AutofillClient::AddressProfileSavePromptCallback
                          save_address_profile_callback,
                      PrimaryActionCallback primary_action_callback);
  bool IsMessageDisplayed();
  // Called in response to user clicking the primary button.
  void OnPrimaryAction();
  // Called whenever the message is dismissed (e.g. after timeout or because the
  // user already accepted or declined the message).
  void OnMessageDismissed(messages::DismissReason dismiss_reason);

  void DismissMessageForTest(messages::DismissReason reason);

 private:
  friend class SaveUpdateAddressProfileMessageControllerTest;

  void DismissMessage();

  void RunSaveAddressProfileCallback(
      AutofillClient::AddressPromptUserDecision decision);

  bool UserSignedIn() const;
  std::u16string GetTitle();
  std::u16string GetDescription();
  std::u16string GetRecordTypeNotice();
  std::u16string GetPrimaryButtonText();

  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::unique_ptr<messages::MessageWrapper> message_;

  // The option which specifies whether user's address profile is going to be
  // migrated to their Google Account.
  bool is_migration_to_account_;
  // The profile which is being confirmed by the user.
  std::optional<AutofillProfile> profile_;
  // The profile (if exists) which will be updated if the user confirms.
  raw_ptr<const AutofillProfile> original_profile_;
  // The callback to run once the user makes the final decision.
  AutofillClient::AddressProfileSavePromptCallback
      save_address_profile_callback_;
  PrimaryActionCallback primary_action_callback_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_MESSAGE_CONTROLLER_H_

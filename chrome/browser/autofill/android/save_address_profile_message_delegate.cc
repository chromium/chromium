// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_address_profile_message_delegate.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/messages/android/message_dispatcher_bridge.h"

namespace autofill {

SaveAddressProfileMessageDelegate::SaveAddressProfileMessageDelegate() =
    default;

SaveAddressProfileMessageDelegate::~SaveAddressProfileMessageDelegate() {
  DismissSavePrompt();
}

void SaveAddressProfileMessageDelegate::DisplaySavePrompt(
    content::WebContents* web_contents,
    const AutofillProfile& profile,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  DCHECK(web_contents);
  DCHECK(callback);

  DCHECK(base::FeatureList::IsEnabled(
      autofill::features::kAutofillAddressProfileSavePrompt));

  // Dismiss previous message if it is displayed.
  DismissSavePrompt();
  DCHECK(message_ == nullptr);

  web_contents_ = web_contents;
  profile_ = profile;
  callback_ = std::move(callback);

  // Binding with base::Unretained(this) is safe here because
  // SaveAddressProfileMessageDelegate owns message_. Callbacks won't be called
  // after the current object is destroyed.
  message_ = std::make_unique<messages::MessageWrapper>(
      base::BindOnce(&SaveAddressProfileMessageDelegate::OnSaveClicked,
                     base::Unretained(this)),
      base::BindOnce(&SaveAddressProfileMessageDelegate::OnMessageDismissed,
                     base::Unretained(this)));

  // TODO(crbug.com/1167061): Replace with proper localized string.
  message_->SetTitle(u"Save address?");
  message_->SetDescription(u"Fill forms faster in Chrome");
  message_->SetPrimaryButtonText(u"Save...");

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::WEB_CONTENTS);
}

void SaveAddressProfileMessageDelegate::DismissSavePrompt() {
  if (message_ != nullptr) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), web_contents_, messages::DismissReason::UNKNOWN);
  }
}

void SaveAddressProfileMessageDelegate::OnSaveClicked() {
  RunCallback(AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted);
}

void SaveAddressProfileMessageDelegate::OnMessageDismissed(
    messages::DismissReason dismiss_reason) {
  switch (dismiss_reason) {
    case messages::DismissReason::PRIMARY_ACTION:
    case messages::DismissReason::SECONDARY_ACTION:
      // Primary action is handled separately, no secondary action.
      break;
    case messages::DismissReason::GESTURE:
      // User explicitly dismissed the message.
      RunCallback(
          AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
      break;
    default:
      // Dismissal without direct interaction.
      RunCallback(
          AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored);
      break;
  }

  // Clean the state, message is no longer shown.
  message_.reset();
  web_contents_ = nullptr;
}

void SaveAddressProfileMessageDelegate::RunCallback(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  std::move(callback_).Run(decision, profile_);
}

}  // namespace autofill

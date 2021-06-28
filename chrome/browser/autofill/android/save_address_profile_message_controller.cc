// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_address_profile_message_controller.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveAddressProfileMessageController::SaveAddressProfileMessageController() =
    default;

SaveAddressProfileMessageController::~SaveAddressProfileMessageController() {
  DismissMessage();
}

void SaveAddressProfileMessageController::DisplayMessage(
    content::WebContents* web_contents,
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    AutofillClient::AddressProfileSavePromptCallback
        save_address_profile_callback,
    PrimaryActionCallback primary_action_callback) {
  DCHECK(web_contents);
  DCHECK(save_address_profile_callback);
  DCHECK(primary_action_callback);

  DCHECK(base::FeatureList::IsEnabled(
      autofill::features::kAutofillAddressProfileSavePrompt));

  // Dismiss previous message if it is displayed.
  DismissMessage();
  DCHECK(!message_);

  web_contents_ = web_contents;
  profile_ = profile;
  original_profile_ = original_profile;
  save_address_profile_callback_ = std::move(save_address_profile_callback);
  primary_action_callback_ = std::move(primary_action_callback);

  // Binding with base::Unretained(this) is safe here because
  // SaveAddressProfileMessageController owns message_. Callbacks won't be
  // called after the current object is destroyed.
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::SAVE_ADDRESS_PROFILE,
      base::BindOnce(&SaveAddressProfileMessageController::OnPrimaryAction,
                     base::Unretained(this)),
      base::BindOnce(&SaveAddressProfileMessageController::OnMessageDismissed,
                     base::Unretained(this)));

  message_->SetTitle(GetTitle());
  message_->SetDescription(GetDescription());
  message_->SetDescriptionMaxLines(1);
  message_->SetPrimaryButtonText(GetPrimaryButtonText());
  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_ADDRESS));

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kNormal);
}

bool SaveAddressProfileMessageController::IsMessageDisplayed() {
  return !!message_;
}

void SaveAddressProfileMessageController::OnPrimaryAction() {
  std::move(primary_action_callback_)
      .Run(web_contents_, profile_, original_profile_,
           std::move(save_address_profile_callback_));
}

void SaveAddressProfileMessageController::OnMessageDismissed(
    messages::DismissReason dismiss_reason) {
  switch (dismiss_reason) {
    case messages::DismissReason::PRIMARY_ACTION:
    case messages::DismissReason::SECONDARY_ACTION:
      // Primary action is handled separately, no secondary action.
      break;
    case messages::DismissReason::GESTURE:
      // User explicitly dismissed the message.
      RunSaveAddressProfileCallback(
          AutofillClient::SaveAddressProfileOfferUserDecision::
              kMessageDeclined);
      break;
    case messages::DismissReason::TIMER:
      // The message was auto-dismissed after a timeout.
      RunSaveAddressProfileCallback(
          AutofillClient::SaveAddressProfileOfferUserDecision::kMessageTimeout);
      break;
    default:
      // Dismissal for any other reason.
      RunSaveAddressProfileCallback(
          AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored);
      break;
  }

  // Clean the state, message is no longer shown.
  message_.reset();
  web_contents_ = nullptr;
}

void SaveAddressProfileMessageController::DismissMessageForTest(
    messages::DismissReason reason) {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), web_contents_, reason);
  }
}

void SaveAddressProfileMessageController::DismissMessage() {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), web_contents_, messages::DismissReason::UNKNOWN);
  }
}

void SaveAddressProfileMessageController::RunSaveAddressProfileCallback(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  std::move(save_address_profile_callback_).Run(decision, profile_);
  primary_action_callback_.Reset();
}

std::u16string SaveAddressProfileMessageController::GetTitle() {
  return l10n_util::GetStringUTF16(
      original_profile_ ? IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE
                        : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
}

std::u16string SaveAddressProfileMessageController::GetDescription() {
  return GetProfileDescription(
      original_profile_ ? *original_profile_ : profile_,
      g_browser_process->GetApplicationLocale(),
      /*include_address_and_contacts=*/true);
}

std::u16string SaveAddressProfileMessageController::GetPrimaryButtonText() {
  return l10n_util::GetStringUTF16(
      original_profile_ ? IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL
                        : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
}

}  // namespace autofill

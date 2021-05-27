// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_address_profile_flow_manager.h"

#include <utility>

#include "chrome/browser/ui/android/autofill/save_address_profile_prompt_view_android.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/messages/android/messages_feature.h"

namespace autofill {

SaveAddressProfileFlowManager::SaveAddressProfileFlowManager() = default;
SaveAddressProfileFlowManager::~SaveAddressProfileFlowManager() = default;

void SaveAddressProfileFlowManager::OfferSave(
    content::WebContents* web_contents,
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  DCHECK(web_contents);
  DCHECK(callback);
  DCHECK(base::FeatureList::IsEnabled(
      autofill::features::kAutofillAddressProfileSavePrompt));

  // If the message or prompt is already shown, suppress the incoming offer.
  if (save_address_profile_message_controller_.IsMessageDisplayed() ||
      save_address_profile_prompt_controller_) {
    std::move(callback).Run(
        AutofillClient::SaveAddressProfileOfferUserDecision::kAutoDeclined,
        profile);
    return;
  }

  if (base::FeatureList::IsEnabled(
          messages::kMessagesForAndroidInfrastructure)) {
    ShowSaveAddressProfileMessage(web_contents, profile, original_profile,
                                  std::move(callback));
  } else {
    // Fallback to the default behavior without confirmation.
    std::move(callback).Run(
        AutofillClient::SaveAddressProfileOfferUserDecision::kUserNotAsked,
        profile);
  }
}

SaveAddressProfileMessageController*
SaveAddressProfileFlowManager::GetMessageControllerForTest() {
  return &save_address_profile_message_controller_;
}

SaveAddressProfilePromptController*
SaveAddressProfileFlowManager::GetPromptControllerForTest() {
  return save_address_profile_prompt_controller_.get();
}

void SaveAddressProfileFlowManager::ShowSaveAddressProfileMessage(
    content::WebContents* web_contents,
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  save_address_profile_message_controller_.DisplayMessage(
      web_contents, profile, original_profile, std::move(callback),
      base::BindOnce(
          &SaveAddressProfileFlowManager::ShowSaveAddressProfileDetails,
          // Passing base::Unretained(this) is safe since |this|
          // owns the controller.
          base::Unretained(this)));
}

void SaveAddressProfileFlowManager::ShowSaveAddressProfileDetails(
    content::WebContents* web_contents,
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  auto prompt_view_android =
      std::make_unique<SaveAddressProfilePromptViewAndroid>(web_contents);
  save_address_profile_prompt_controller_ =
      std::make_unique<SaveAddressProfilePromptController>(
          std::move(prompt_view_android), profile, original_profile,
          std::move(callback),
          /*dismissal_callback=*/
          base::BindOnce(
              &SaveAddressProfileFlowManager::OnSaveAddressProfileDetailsShown,
              // Passing base::Unretained(this) is safe since |this|
              // owns the controller.
              base::Unretained(this)));
  save_address_profile_prompt_controller_->DisplayPrompt();
}

void SaveAddressProfileFlowManager::OnSaveAddressProfileDetailsShown() {
  save_address_profile_prompt_controller_.reset();
}

}  // namespace autofill

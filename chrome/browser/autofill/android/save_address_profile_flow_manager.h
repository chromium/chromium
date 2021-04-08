// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_FLOW_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_FLOW_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/autofill/android/save_address_profile_message_controller.h"
#include "chrome/browser/autofill/android/save_address_profile_prompt_controller.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// Class to manage save address profile flow on Android. The flow consists
// of 2 steps: showing a confirmation message and a prompt with profile details
// to review. This class owns and triggers the corresponding controllers.
class SaveAddressProfileFlowManager {
 public:
  SaveAddressProfileFlowManager();
  SaveAddressProfileFlowManager(const SaveAddressProfileFlowManager&) = delete;
  SaveAddressProfileFlowManager& operator=(
      const SaveAddressProfileFlowManager&) = delete;
  ~SaveAddressProfileFlowManager();

  void OfferSave(content::WebContents* web_contents,
                 const AutofillProfile& profile,
                 AutofillClient::AddressProfileSavePromptCallback callback);

 private:
  void ShowSaveAddressProfileMessage(
      content::WebContents* web_contents,
      const AutofillProfile& profile,
      AutofillClient::AddressProfileSavePromptCallback callback);

  void ShowSaveAddressProfileDetails(
      content::WebContents* web_contents,
      const AutofillProfile& profile,
      AutofillClient::AddressProfileSavePromptCallback callback);

  void OnSaveAddressProfileDetailsShown();

  SaveAddressProfileMessageController save_address_profile_message_controller_;
  std::unique_ptr<SaveAddressProfilePromptController>
      save_address_profile_prompt_controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_FLOW_MANAGER_H_

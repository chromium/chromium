// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_VIEW_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_VIEW_H_

#include "chrome/browser/autofill/android/save_update_address_profile_prompt_mode.h"

namespace autofill {

class AutofillProfile;
class SaveUpdateAddressProfilePromptController;

// The UI interface which prompts the user to confirm saving new/updating
// existing address profile imported from a form submission.
class SaveUpdateAddressProfilePromptView {
 public:
  virtual bool Show(SaveUpdateAddressProfilePromptController* controller,
                    const AutofillProfile& autofill_profile,
                    SaveUpdateAddressProfilePromptMode prompt_mode) = 0;

  virtual ~SaveUpdateAddressProfilePromptView() = default;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_VIEW_H_

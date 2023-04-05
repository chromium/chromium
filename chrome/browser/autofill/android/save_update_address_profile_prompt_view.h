// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_VIEW_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_VIEW_H_

namespace autofill {

class AutofillProfile;
class SaveUpdateAddressProfilePromptController;

// The UI interface which prompts the user to confirm saving new/updating
// existing address profile imported from a form submission.
class SaveUpdateAddressProfilePromptView {
 public:
  virtual bool Show(SaveUpdateAddressProfilePromptController* controller,
                    const AutofillProfile& autofill_profile,
                    bool is_update,
                    bool is_migration_to_account) = 0;

  virtual ~SaveUpdateAddressProfilePromptView() = default;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_VIEW_H_

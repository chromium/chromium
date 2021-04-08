// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_PROMPT_VIEW_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_PROMPT_VIEW_H_

#include "base/macros.h"

namespace autofill {

class SaveAddressProfilePromptController;

// The UI interface which prompts the user to confirm saving an address profile
// imported from a form submission.
class SaveAddressProfilePromptView {
 public:
  virtual bool Show(SaveAddressProfilePromptController* controller) = 0;

  virtual ~SaveAddressProfilePromptView() = default;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_PROMPT_VIEW_H_

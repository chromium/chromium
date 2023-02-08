// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_AUTOFILL_PROFILE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_AUTOFILL_PROFILE_BRIDGE_H_

namespace content {
class WebContents;
}

namespace autofill {

// Opens the autofill settings page for profiles.
void ShowAutofillProfileSettings(content::WebContents* web_contents);

// Opens the autofill settings page for credit cards.
void ShowAutofillCreditCardSettings(content::WebContents* web_contents);

}  // namespace autofill

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_AUTOFILL_PROFILE_BRIDGE_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AI_AUTOFILL_AI_UTIL_H_
#define CHROME_BROWSER_AUTOFILL_AI_AUTOFILL_AI_UTIL_H_

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace autofill_ai {

bool IsUserEligible(Profile* profile);

// The Autofill Ai page is shown only if the user is eligible or if they have
// any entity instances saved.
bool CanShowAutofillAiPageInSettings(Profile* profile,
                                     content::WebContents* web_contents);

}  // namespace autofill_ai

#endif  // CHROME_BROWSER_AUTOFILL_AI_AUTOFILL_AI_UTIL_H_

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_UTIL_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_UTIL_H_

#include <vector>

class Profile;

namespace autofill {

class AutofillExternalDelegate;
class AutofillProfile;
class CreditCard;

void AddTestProfile(Profile* base_profile, const AutofillProfile& profile);
void SetTestProfile(Profile* base_profile, const AutofillProfile& profile);
void SetTestProfiles(Profile* base_profile,
                     std::vector<AutofillProfile>* profiles);
void AddTestCreditCard(Profile* base_profile, const CreditCard& card);
void AddTestServerCreditCard(Profile* base_profile, const CreditCard& card);
void AddTestAutofillData(Profile* base_profile,
                         const AutofillProfile& profile,
                         const CreditCard& card);
void WaitForPersonalDataChange(Profile* base_profile);
void WaitForPersonalDataManagerToBeLoaded(Profile* base_profile);

// Displays an Autofill popup with a dummy suggestion.
// Unlike autofill::test::GenerateTestAutofillPopup(), this function triggers
// the popup through the `autofill_external_delegate->GetAutofillDriver()`'s
// AskForValuesToFill(), instead of the |autofill_external_delegate|'s
// OnQuery() event. This initializes the form's meta data and prepares
// ContentAutofillDriver's and ContentAutofillRouter's state to process events
// such as AutofillDriver::PopupHidden() triggered by the popup.
void GenerateTestAutofillPopup(
    AutofillExternalDelegate* autofill_external_delegate);

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_UTIL_H_

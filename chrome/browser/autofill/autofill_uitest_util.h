// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_UTIL_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_UTIL_H_

#include <vector>

class Profile;

namespace autofill {

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

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_UTIL_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_SETTINGS_NAVIGATION_HELPER_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_SETTINGS_NAVIGATION_HELPER_H_

namespace content {
class WebContents;
}

namespace autofill {

// Needs to stay in sync with AutofillOptionsReferrer in enums.xml.
// LINT.IfChange(AutofillOptionsReferrer)
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.autofill.settings.options
enum class AutofillOptionsReferrer {
  // Corresponds to the Settings page.
  kSettings = 0,

  // Corresponds to an external link opening Chrome.
  kDeepLinkToSettings = 1,

  // Payment methods fragment in Chrome settings.
  kPaymentMethodsFragment = 2,

  // Profiles fragment in Chrome settings.
  kAutofillProfilesFragment = 3,

  // Autofill and passwords in Chrome settings.
  kAutofillAndPasswordsFragment = 4,

  // Identity docs fragment in Chrome settings.
  kAutofillIdentityDocsFragment = 5,

  // Travel fragment in Chrome settings.
  kAutofillTravelFragment = 6,

  // Shopping fragment in Chrome settings.
  kAutofillShoppingFragment = 7,

  // Private inference notice.
  kPrivateInferenceNotice = 8,

  // Personal context AtMemory notice.
  kPersonalContextAtmemoryNotice = 9,

  // Personal context ambient autofill notice.
  kPersonalContextAmbientAutofillNotice = 10,

  kMaxValue = kPersonalContextAmbientAutofillNotice,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillOptionsReferrer)

// Opens the autofill settings page for profiles.
void ShowAutofillProfileSettings(content::WebContents* web_contents);

// Opens the autofill settings page for credit cards.
void ShowAutofillCreditCardSettings(content::WebContents* web_contents);

// Opens the autofill settings page for identity docs.
void ShowAutofillIdentityDocsSettings(content::WebContents* web_contents);

// Opens the autofill settings page for travel.
void ShowAutofillTravelSettings(content::WebContents* web_contents);

// Opens the autofill settings page for shopping.
void ShowAutofillShoppingSettings(content::WebContents* web_contents);

// Opens the autofill settings page for personal context.
void ShowAutofillPersonalContextSettings(content::WebContents* web_contents,
                                         AutofillOptionsReferrer referrer);

// Open the autofill settings page.
void ShowAutofillSettingsPage(content::WebContents* web_contents);

}  // namespace autofill

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_SETTINGS_NAVIGATION_HELPER_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_ENUMS_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_ENUMS_H_

// Enum that describes different outcomes to an attempt of triggering the
// FastCheckout bottomsheet.
// Do not remove or renumber entries in this enum. It needs to be kept in
// sync with the enum of the same name in `enums.xml`.
// The enum values are not exhaustive to avoid excessive metric collection.
// Instead focus on the most interesting abort cases and only deal with cases
// in which the FastCheckout feature is enabled and a script exists for the
// form in question.
enum class FastCheckoutTriggerOutcome {
  // The sheet was shown.
  kSuccess = 0,
  // The sheet was not shown because it has already been shown before.
  kFailureShownBefore = 1,
  // The sheet was not shown because the clicked field is not focusable.
  kFailureFieldNotFocusable = 2,
  // The sheet was not shown because the clicked field is not empty.
  kFailureFieldNotEmpty = 3,
  // The sheet was not shown because Autofill UI cannot be shown.
  kFailureCannotShowAutofillUi = 4,
  // The sheet was not shown because there is no valid credit card.
  kFailureNoValidCreditCard = 5,
  // The sheet was not shown because there is no valid Autofill profile.
  kFailureNoValidAutofillProfile = 6,
  kMaxValue = kFailureNoValidAutofillProfile
};

// Enum defining possible outcomes of a Fast Checkout run. Must be kept in sync
// with enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// TODO(crbug.com/1334642): remove references to scripts and adjust to new
// implementation.
enum class FastCheckoutRunOutcome {
  // Script did not run because the user has declined onboarding.
  kOnboardingDeclined = 0,
  // The script run did not complete or never started.
  kIncompleteRun = 1,
  // Script run failed.
  kFail = 2,
  // Script ran successfully.
  kSuccess = 3,
  kMaxValue = kSuccess
};

// Represents the state of the bottomsheet.
enum class FastCheckoutUIState {
  kNotShownYet,
  kIsShowing,
  kWasShown,
};

#endif

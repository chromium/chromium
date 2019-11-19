// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;

/**
 * Common interface for autofill assistant payment request delegates.
 *
 * Methods in this delegate are automatically invoked by the PR UI as the user interacts with the
 * UI.
 */
public interface AssistantCollectUserDataDelegate {
    /** The currently selected contact has changed. */
    void onContactInfoChanged(@Nullable AutofillContact contact);

    /** The currently selected shipping address has changed. */
    void onShippingAddressChanged(@Nullable AutofillAddress address);

    /** The currently selected payment method has changed. */
    void onPaymentMethodChanged(@Nullable AutofillPaymentInstrument paymentInstrument);

    /** The currently selected terms & conditions state has changed. */
    void onTermsAndConditionsChanged(@AssistantTermsAndConditionsState int state);

    /** Called when a link on the terms and conditions message is clicked. */
    void onTermsAndConditionsLinkClicked(int link);

    /** The currently selected login choice has changed. */
    void onLoginChoiceChanged(@Nullable AssistantLoginChoice loginChoice);

    /** The start of the date/time range has changed. */
    void onDateTimeRangeStartChanged(
            int year, int month, int day, int hour, int minute, int second);

    /** The end of the date/time range has changed. */
    void onDateTimeRangeEndChanged(int year, int month, int day, int hour, int minute, int second);

    /** The value of a key/value pair has changed. */
    void onKeyValueChanged(String key, String value);
}

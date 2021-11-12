// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantValue;

/**
 * Common interface for autofill assistant payment request delegates.
 *
 * Methods in this delegate are automatically invoked by the PR UI as the user interacts with the
 * UI.
 */
public interface AssistantCollectUserDataDelegate {
    /** The currently selected contact has changed. */
    void onContactInfoChanged(@Nullable AssistantCollectUserDataModel.ContactModel contactModel,
            @AssistantUserDataEventType int eventType);

    /** The currently selected shipping address has changed. */
    void onShippingAddressChanged(@Nullable AssistantCollectUserDataModel.AddressModel addressModel,
            @AssistantUserDataEventType int eventType);

    /** The currently selected payment method has changed. */
    void onPaymentMethodChanged(
            @Nullable AssistantCollectUserDataModel.PaymentInstrumentModel paymentInstrumentModel,
            @AssistantUserDataEventType int eventType);

    /** The currently selected terms & conditions state has changed. */
    void onTermsAndConditionsChanged(@AssistantTermsAndConditionsState int state);

    /** Called when a text link of the form <link0>text</link0>in a message is clicked. */
    void onTextLinkClicked(int link);

    /** The currently selected login choice has changed. */
    void onLoginChoiceChanged(
            @Nullable AssistantCollectUserDataModel.LoginChoiceModel loginChoiceModel,
            @AssistantUserDataEventType int eventType);

    /** The start date of the date/time range has changed. */
    void onDateTimeRangeStartDateChanged(@Nullable AssistantDateTime date);

    /** The start time of the date/time range has changed. */
    void onDateTimeRangeStartTimeSlotChanged(@Nullable Integer index);

    /** The start date of the date/time range has changed. */
    void onDateTimeRangeEndDateChanged(@Nullable AssistantDateTime date);

    /** The end time of the date/time range has changed. */
    void onDateTimeRangeEndTimeSlotChanged(@Nullable Integer index);

    /** The value of a key/value pair has changed. */
    void onKeyValueChanged(String key, AssistantValue value);

    /** The focus on an input text field has changed */
    void onInputTextFocusChanged(boolean isFocused);
}

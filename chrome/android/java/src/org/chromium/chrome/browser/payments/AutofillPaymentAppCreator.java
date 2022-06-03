// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.components.payments.PaymentApp;

/** The interface for creating payment apps from Autofill cards. */
public interface AutofillPaymentAppCreator {
    /**
     * Creates a payment app for the given Autofill card if it is usable for the payment request.
     * Called when personal data manager updates a card on file while the payment UI is being
     * displayed.
     *
     * @param card The card for which the payment app is to be created.
     * @return The payment app for the given card. Null if the card is not usable for payment
     * request.
     */
    @Nullable
    PaymentApp createPaymentAppForCard(CreditCard card);
}

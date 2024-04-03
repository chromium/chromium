// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the logic for the facilitated payments component. It sets the state of the model and
 * reacts to events like clicks.
 */
class FacilitatedPaymentsPaymentMethodsMediator {
    private PropertyModel mModel;

    void initialize(PropertyModel model) {
        mModel = model;
    }

    void showSheet() {
        mModel.set(VISIBLE, true);
    }
}

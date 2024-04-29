// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/**
 * Properties defined here reflect the visible state of the facilitated payments bottom sheet
 * component.
 */
class FacilitatedPaymentsPaymentMethodsProperties {
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey("visible");
    static final ReadableObjectPropertyKey<ModelList> SHEET_ITEMS =
            new ReadableObjectPropertyKey("sheet_items");

    static final PropertyKey[] ALL_KEYS = {VISIBLE, SHEET_ITEMS};

    @interface ItemType {
        // The header at the top of the FacilitatedPayments bottom sheet.
        int HEADER = 0;

        // A section containing the payment instrument data.
        int PAYMENT_INSTRUMENT = 1;

        // A footer section containing additional actions.
        int FOOTER = 2;
    }

    /** Properties for a payment instrument entry in the facilitated payments bottom sheet. */
    static class PaymentInstrumentProperties {
        static final PropertyKey[] NON_TRANSFORMING_KEYS = {};

        private PaymentInstrumentProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the header in the facilitated payments
     * bottom sheet for payments.
     */
    static class HeaderProperties {
        static final ReadableIntPropertyKey IMAGE_DRAWABLE_ID =
                new ReadableIntPropertyKey("image_drawable_id");
        static final ReadableIntPropertyKey TITLE_ID = new ReadableIntPropertyKey("title_id");
        static final ReadableIntPropertyKey DESCRIPTION_ID =
                new ReadableIntPropertyKey("description_id");
        static final PropertyKey[] ALL_KEYS = {IMAGE_DRAWABLE_ID, TITLE_ID, DESCRIPTION_ID};

        private HeaderProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the footer in the facilitated payments
     * bottom sheet for payments.
     */
    static class FooterProperties {
        static final PropertyKey[] ALL_KEYS = {};

        private FooterProperties() {}
    }

    private FacilitatedPaymentsPaymentMethodsProperties() {}
}

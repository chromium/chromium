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

        // A section containing the bank account data.
        int BANK_ACCOUNT = 1;

        // Additional info to users making payments through bottom sheet.
        int ADDITIONAL_INFO = 2;

        // A footer section containing additional actions.
        int FOOTER = 3;
    }

    /** Properties for a payment instrument entry in the facilitated payments bottom sheet. */
    static class BankAccountProperties {
        static final ReadableObjectPropertyKey<String> BANK_NAME =
                new ReadableObjectPropertyKey("bank_name");
        static final ReadableObjectPropertyKey<String> BANK_ACCOUNT_SUMMARY =
                new ReadableObjectPropertyKey("bank_account_summary");
        static final ReadableIntPropertyKey BANK_ACCOUNT_DRAWABLE_ID =
                new ReadableIntPropertyKey("bank_account_drawable_id");

        static final PropertyKey[] NON_TRANSFORMING_KEYS = {
            BANK_NAME, BANK_ACCOUNT_SUMMARY, BANK_ACCOUNT_DRAWABLE_ID
        };

        private BankAccountProperties() {}
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
     * Properties defined here reflect the visible state of the additional info in the facilitated
     * payments bottom sheet for payments.
     */
    static class AdditionalInfoProperties {
        static final ReadableIntPropertyKey DESCRIPTION_1_ID =
                new ReadableIntPropertyKey("description_1_id");

        static final PropertyKey[] ALL_KEYS = {DESCRIPTION_1_ID};

        private AdditionalInfoProperties() {}
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

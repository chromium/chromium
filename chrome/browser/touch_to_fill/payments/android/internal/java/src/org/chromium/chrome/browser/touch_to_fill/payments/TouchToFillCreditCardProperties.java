// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/**
 * Properties defined here reflect the visible state of the TouchToFillCreditCard component.
 */
class TouchToFillCreditCardProperties {
    static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");
    public static final PropertyModel.ReadableObjectPropertyKey<ModelList> SHEET_ITEMS =
            new PropertyModel.ReadableObjectPropertyKey("sheet_items");
    static final PropertyModel.ReadableObjectPropertyKey<Callback<Integer>> DISMISS_HANDLER =
            new PropertyModel.ReadableObjectPropertyKey<>("dismiss_handler");
    static final PropertyModel.WritableBooleanPropertyKey SHOULD_SHOW_SCAN_CREDIT_CARD =
            new PropertyModel.WritableBooleanPropertyKey("should_show_scan_credit_card");
    static final PropertyModel.ReadableObjectPropertyKey<Runnable> SCAN_CREDIT_CARD_CALLBACK =
            new ReadableObjectPropertyKey<>("scan_credit_card_callback");
    static final PropertyModel
            .ReadableObjectPropertyKey<Runnable> SHOW_CREDIT_CARD_SETTINGS_CALLBACK =
            new ReadableObjectPropertyKey<>("show_credit_card_settings_callback");

    static final PropertyKey[] ALL_KEYS = {VISIBLE, SHEET_ITEMS, DISMISS_HANDLER,
            SHOULD_SHOW_SCAN_CREDIT_CARD, SCAN_CREDIT_CARD_CALLBACK,
            SHOW_CREDIT_CARD_SETTINGS_CALLBACK};

    @interface ItemType {
        // A section containing the credit card data.
        int CREDIT_CARD = 0;
    }

    /**
     * Properties for a credit card entry in the TouchToFill sheet for payments.
     */
    static class CreditCardProperties {
        static final PropertyModel.ReadableIntPropertyKey CARD_ICON_ID =
                new PropertyModel.ReadableIntPropertyKey("card_icon_id");
        static final PropertyModel.ReadableObjectPropertyKey<String> CARD_NAME =
                new PropertyModel.ReadableObjectPropertyKey<>("card_name");
        static final PropertyModel.ReadableObjectPropertyKey<String> CARD_NUMBER =
                new PropertyModel.ReadableObjectPropertyKey<>("card_number");
        static final PropertyModel.ReadableObjectPropertyKey<String> CARD_EXPIRATION =
                new PropertyModel.ReadableObjectPropertyKey<>("card_expiration");
        static final PropertyModel.ReadableObjectPropertyKey<Runnable> ON_CLICK_ACTION =
                new PropertyModel.ReadableObjectPropertyKey<>("on_click_action");

        static final PropertyKey[] ALL_KEYS = {
                CARD_ICON_ID, CARD_NAME, CARD_NUMBER, CARD_EXPIRATION, ON_CLICK_ACTION};

        private CreditCardProperties() {}
    }

    private TouchToFillCreditCardProperties() {}
}

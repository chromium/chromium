// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.graphics.Bitmap;
import android.view.View.OnClickListener;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Properties defined here reflect the visible state of the facilitated payments bottom sheet
 * component.
 */
class FacilitatedPaymentsPaymentMethodsProperties {
    static final WritableIntPropertyKey VISIBLE_STATE = new WritableIntPropertyKey("visible_state");
    static final WritableIntPropertyKey SCREEN = new WritableIntPropertyKey("screen");
    static final WritableObjectPropertyKey<PropertyModel> SCREEN_VIEW_MODEL =
            new WritableObjectPropertyKey("screen_view_model");
    static final ReadableObjectPropertyKey<Callback<Integer>> DISMISS_HANDLER =
            new ReadableObjectPropertyKey<>("dismiss_handler");

    static final PropertyKey[] ALL_KEYS = {
        VISIBLE_STATE, SCREEN, SCREEN_VIEW_MODEL, DISMISS_HANDLER
    };

    // TODO: b/348595414 - Rename to FopSelectorItemType and move to a separate directory.
    @interface ItemType {
        // The header at the top of the FacilitatedPayments bottom sheet.
        int HEADER = 0;

        // A section containing the bank account data.
        int BANK_ACCOUNT = 1;

        // Additional info to users making payments through bottom sheet.
        int ADDITIONAL_INFO = 2;

        // A "Continue" button, which is shown when there is only one payment
        // method available.
        int CONTINUE_BUTTON = 3;

        // A footer section containing additional actions.
        int FOOTER = 4;
    }

    // The visible state of the Facilitated Payments bottom sheet.
    @interface VisibleState {
        // The bottom sheet is not open. This is the default state.
        int HIDDEN = 0;
        // The bottom sheet is open and showing a screen.
        int SHOWN = 1;
        // The bottom sheet is in a temporary transition state before showing a new screen. The
        // bottom sheet can come to this temporary state from either of other 2 states, but will
        // always transition to the {@link SHOWN} state from here. Before showing a new screen, the
        // controller has to set this state.
        int SWAPPING_SCREEN = 2;
    }

    // The type of user visible screens that can be shown in the Facilitated Payments bottom sheet.
    @interface SequenceScreen {
        // No screen should be assigned 0 because {@link WritableIntPropertyKey} defaults to 0.
        int UNINITIALIZED = 0;
        // The screen showing the user's payment instruments.
        int FOP_SELECTOR = 1;
        // The screen showing a progress spinner.
        int PROGRESS_SCREEN = 2;
        // The screen showing an error message.
        int ERROR_SCREEN = 3;
    }

    /**
     * Properties defined here reflect the visible state of the FOP selector view shown in a bottom
     * sheet.
     */
    static class FopSelectorProperties {
        /** A list containing all the view items. They will be shown in a {@link RecyclerView}. */
        static final ReadableObjectPropertyKey<ModelList> SCREEN_ITEMS =
                new ReadableObjectPropertyKey("screen_items");

        /** All the properties of FOP selector screen. */
        static final PropertyKey[] ALL_KEYS = {SCREEN_ITEMS};

        private FopSelectorProperties() {}
    }

    /** Properties for a payment instrument entry in the facilitated payments bottom sheet. */
    static class BankAccountProperties {
        static final ReadableObjectPropertyKey<String> BANK_NAME =
                new ReadableObjectPropertyKey("bank_name");
        static final ReadableObjectPropertyKey<String> BANK_ACCOUNT_SUMMARY =
                new ReadableObjectPropertyKey("bank_account_summary");
        static final ReadableObjectPropertyKey<String> BANK_ACCOUNT_TRANSACTION_LIMIT =
                new ReadableObjectPropertyKey("bank_account_transaction_limit");
        static final ReadableIntPropertyKey BANK_ACCOUNT_DRAWABLE_ID =
                new ReadableIntPropertyKey("bank_account_drawable_id");
        static final ReadableObjectPropertyKey<Runnable> ON_BANK_ACCOUNT_CLICK_ACTION =
                new ReadableObjectPropertyKey<>("on_bank_account_click_action");
        static final ReadableObjectPropertyKey<Bitmap> BANK_ACCOUNT_ICON_BITMAP =
                new ReadableObjectPropertyKey<>("bank_account_icon_bitmap");
        static final PropertyKey[] NON_TRANSFORMING_KEYS = {
            BANK_NAME,
            BANK_ACCOUNT_SUMMARY,
            BANK_ACCOUNT_TRANSACTION_LIMIT,
            BANK_ACCOUNT_DRAWABLE_ID,
            ON_BANK_ACCOUNT_CLICK_ACTION,
            BANK_ACCOUNT_ICON_BITMAP
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
        static final ReadableIntPropertyKey DESCRIPTION_ID =
                new ReadableIntPropertyKey("additional_info_description_id");
        static final ReadableObjectPropertyKey<Runnable> SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK =
                new ReadableObjectPropertyKey<>("show_payment_method_settings_callback");

        static final PropertyKey[] ALL_KEYS = {
            DESCRIPTION_ID, SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK
        };

        private AdditionalInfoProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the footer in the facilitated payments
     * bottom sheet for payments.
     */
    static class FooterProperties {
        static final PropertyModel.ReadableObjectPropertyKey<Runnable>
                SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK =
                        new ReadableObjectPropertyKey<>("show_payment_method_settings_callback");

        static final PropertyKey[] ALL_KEYS = {SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK};

        private FooterProperties() {}
    }

    /**
     * Properties defined here reflect the visible state of the error screen shown in a bottom
     * sheet.
     */
    static class ErrorScreenProperties {
        /** Primary button callback. */
        static final WritableObjectPropertyKey<OnClickListener> PRIMARY_BUTTON_CALLBACK =
                new WritableObjectPropertyKey<>("primary_button_callback");

        /** All the properties of error screen. */
        static final PropertyKey[] ALL_KEYS = {PRIMARY_BUTTON_CALLBACK};

        private ErrorScreenProperties() {}
    }

    private FacilitatedPaymentsPaymentMethodsProperties() {}
}

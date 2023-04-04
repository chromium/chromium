// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.CARD_EXPIRATION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.CARD_ICON_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.CARD_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.CARD_NUMBER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.NETWORK_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.ON_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.VIRTUAL_CARD_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.FooterProperties.SCAN_CREDIT_CARD_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.FooterProperties.SHOULD_SHOW_SCAN_CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.FooterProperties.SHOW_CREDIT_CARD_SETTINGS_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.VISIBLE;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link TouchToFillCreditCardProperties} changes in a {@link
 * PropertyModel} to the suitable method in {@link TouchToFillCreditCardView}.
 */
class TouchToFillCreditCardViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link TouchToFillCreditCardView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindTouchToFillCreditCardView(
            PropertyModel model, TouchToFillCreditCardView view, PropertyKey propertyKey) {
        if (propertyKey == DISMISS_HANDLER) {
            view.setDismissHandler(model.get(DISMISS_HANDLER));
        } else if (propertyKey == VISIBLE) {
            boolean visibilityChangeSuccessful = view.setVisible(model.get(VISIBLE));
            if (!visibilityChangeSuccessful && model.get(VISIBLE)) {
                assert (model.get(DISMISS_HANDLER) != null);
                model.get(DISMISS_HANDLER).onResult(BottomSheetController.StateChangeReason.NONE);
            }
        } else if (propertyKey == SHEET_ITEMS) {
            TouchToFillCreditCardCoordinator.setUpCardItems(model, view);
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private TouchToFillCreditCardViewBinder() {}

    /**
     * Factory used to create a card item inside the ListView inside the TouchToFillCreditCardView.
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createCardItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_credit_card_sheet_item, parent, false);
    }

    /** Binds the item view to the model properties. */
    static void bindCardItemView(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView cardName = view.findViewById(R.id.card_name);
        if (propertyKey == CARD_ICON_ID) {
            ImageView icon = view.findViewById(R.id.favicon);
            int iconId = model.get(CARD_ICON_ID);
            // Generally the resource id for the icon can only be zero in the tests.
            // For production code a general card icon id is set by default in the CreditCard
            // constructor if the card issuer is unknown.
            icon.setImageDrawable(
                    iconId != 0 ? AppCompatResources.getDrawable(view.getContext(), iconId) : null);
        } else if (propertyKey == NETWORK_NAME) {
            if (!model.get(NETWORK_NAME).isEmpty()) {
                cardName.setContentDescription(
                        model.get(CARD_NAME) + " " + model.get(NETWORK_NAME));
            }
        } else if (propertyKey == CARD_NAME) {
            cardName.setText(model.get(CARD_NAME));
        } else if (propertyKey == CARD_NUMBER) {
            TextView cardNumber = view.findViewById(R.id.card_number);
            cardNumber.setText(model.get(CARD_NUMBER));
        } else if (propertyKey == CARD_EXPIRATION) {
            TextView expirationDate = view.findViewById(R.id.description_line_2);
            expirationDate.setText(model.get(CARD_EXPIRATION));
        } else if (propertyKey == VIRTUAL_CARD_LABEL) {
            TextView virtualCardLabel = view.findViewById(R.id.description_line_2);
            virtualCardLabel.setText(model.get(VIRTUAL_CARD_LABEL));
        } else if (propertyKey == ON_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_CLICK_ACTION).run());
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new header inside the ListView inside the TouchToFillCreditCardView.
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createHeaderItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_credit_card_header_item, parent, false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindHeaderView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == IMAGE_DRAWABLE_ID) {
            ImageView sheetHeaderImage = view.findViewById(R.id.branding_icon);
            sheetHeaderImage.setImageDrawable(AppCompatResources.getDrawable(
                    view.getContext(), model.get(IMAGE_DRAWABLE_ID)));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    static View createFillButtonView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_fill_button_modern, parent, false);
    }

    static void bindFillButtonView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ON_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_CLICK_ACTION).run());
            TextView buttonTitleText = view.findViewById(R.id.touch_to_fill_button_title);
            buttonTitleText.setText(R.string.autofill_credit_card_continue_button);
        } else if (propertyKey == CARD_ICON_ID || propertyKey == NETWORK_NAME
                || propertyKey == CARD_NAME || propertyKey == CARD_NUMBER
                || propertyKey == CARD_EXPIRATION || propertyKey == VIRTUAL_CARD_LABEL) {
            // Skip, because none of these changes affect the button
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new footer inside the ListView inside the TouchToFillCreditCardView.
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createFooterItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_credit_card_footer_item, parent, false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindFooterView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == SHOULD_SHOW_SCAN_CREDIT_CARD) {
            setScanCreditCardButton(view, model.get(SHOULD_SHOW_SCAN_CREDIT_CARD));
        } else if (propertyKey == SCAN_CREDIT_CARD_CALLBACK) {
            setScanCreditCardCallback(view, model.get(SCAN_CREDIT_CARD_CALLBACK));
        } else if (propertyKey == SHOW_CREDIT_CARD_SETTINGS_CALLBACK) {
            setShowCreditCardSettingsCallback(view, model.get(SHOW_CREDIT_CARD_SETTINGS_CALLBACK));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private static void setScanCreditCardButton(View view, boolean shouldShowScanCreditCard) {
        View scanCreditCard = view.findViewById(R.id.scan_new_card);
        if (shouldShowScanCreditCard) {
            scanCreditCard.setVisibility(View.VISIBLE);
        } else {
            scanCreditCard.setVisibility(View.GONE);
            scanCreditCard.setOnClickListener(null);
        }
    }

    private static void setScanCreditCardCallback(View view, Runnable callback) {
        View scanCreditCard = view.findViewById(R.id.scan_new_card);
        scanCreditCard.setOnClickListener(unused -> callback.run());
    }

    private static void setShowCreditCardSettingsCallback(View view, Runnable callback) {
        View managePaymentMethodsButton = view.findViewById(R.id.manage_payment_methods);
        managePaymentMethodsButton.setOnClickListener(unused -> callback.run());
    }
}

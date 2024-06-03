// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardProperties.CARD_EXPIRATION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardProperties.CARD_IMAGE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardProperties.CARD_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardProperties.CARD_NUMBER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardProperties.IS_ACCEPTABLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardProperties.ITEM_COLLECTION_INFO;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardProperties.NETWORK_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardProperties.ON_CREDIT_CARD_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardProperties.VIRTUAL_CARD_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SCAN_CREDIT_CARD_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SHOULD_SHOW_SCAN_CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_NICKNAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_VALUE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.ON_IBAN_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.VISIBLE;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link TouchToFillPaymentMethodProperties} changes in a {@link
 * PropertyModel} to the suitable method in {@link TouchToFillPaymentMethodView}.
 */
class TouchToFillPaymentMethodViewBinder {
    private static final float GRAYED_OUT_OPACITY_ALPHA = 0.38f;
    private static final float COMPLETE_OPACITY_ALPHA = 1.0f;

    /**
     * The collection info is added by setting an instance of this delegate on the last text view
     * (it is important to sound naturally and mimic the default message), so that the message which
     * is built from item children gets a suffix like ", 1 of 3.". This delegate also assumes that
     * its host sets text on {@link AccessibilityNodeInfo}, which is true for {@link TextView}.
     */
    private static class TextViewCollectionInfoAccessibilityDelegate
            extends View.AccessibilityDelegate {
        private FillableItemCollectionInfo mCollectionInfo;

        public TextViewCollectionInfoAccessibilityDelegate(
                @NonNull FillableItemCollectionInfo collectionInfo) {
            mCollectionInfo = collectionInfo;
        }

        @Override
        public void onInitializeAccessibilityNodeInfo(
                @NonNull View host, @NonNull AccessibilityNodeInfo info) {
            super.onInitializeAccessibilityNodeInfo(host, info);

            assert info.getText() != null;
            info.setContentDescription(
                    host.getContext()
                            .getString(
                                    R.string.autofill_payment_method_a11y_item_collection_info,
                                    info.getText(),
                                    mCollectionInfo.getPosition(),
                                    mCollectionInfo.getTotal()));
        }
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link TouchToFillPaymentMethodView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindTouchToFillPaymentMethodView(
            PropertyModel model, TouchToFillPaymentMethodView view, PropertyKey propertyKey) {
        if (propertyKey == DISMISS_HANDLER) {
            view.setDismissHandler(model.get(DISMISS_HANDLER));
        } else if (propertyKey == VISIBLE) {
            boolean visibilityChangeSuccessful = view.setVisible(model.get(VISIBLE));
            if (!visibilityChangeSuccessful && model.get(VISIBLE)) {
                assert (model.get(DISMISS_HANDLER) != null);
                model.get(DISMISS_HANDLER).onResult(BottomSheetController.StateChangeReason.NONE);
            }
        } else if (propertyKey == SHEET_ITEMS) {
            TouchToFillPaymentMethodCoordinator.setUpCardItems(model, view);
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private TouchToFillPaymentMethodViewBinder() {}

    /**
     * Factory used to create a card item inside the ListView inside the TouchToFillPaymentMethodView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createCardItemView(ViewGroup parent) {
        View cardItem =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.touch_to_fill_credit_card_sheet_item, parent, false);
        AutofillUiUtils.setFilterTouchForSecurity(cardItem);
        return cardItem;
    }

    /**
     * Factory used to create an IBAN item inside the ListView inside the TouchToFillPaymentMethodView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createIbanItemView(ViewGroup parent) {
        View ibanItem =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.touch_to_fill_iban_sheet_item, parent, false);
        AutofillUiUtils.setFilterTouchForSecurity(ibanItem);
        return ibanItem;
    }

    /** Binds the item view to the model properties. */
    static void bindCardItemView(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView cardName = view.findViewById(R.id.card_name);
        TextView cardNumber = view.findViewById(R.id.card_number);
        ImageView icon = view.findViewById(R.id.favicon);
        TextView descriptionLabel = view.findViewById(R.id.description_line_2);
        if (propertyKey == CARD_IMAGE) {
            icon.setImageDrawable(model.get(CARD_IMAGE));
        } else if (propertyKey == NETWORK_NAME) {
            if (!model.get(NETWORK_NAME).isEmpty()) {
                cardName.setContentDescription(
                        model.get(CARD_NAME) + " " + model.get(NETWORK_NAME));
            }
        } else if (propertyKey == CARD_NAME) {
            cardName.setText(model.get(CARD_NAME));
        } else if (propertyKey == CARD_NUMBER) {
            cardNumber.setText(model.get(CARD_NUMBER));
        } else if (propertyKey == CARD_EXPIRATION) {
            descriptionLabel.setText(model.get(CARD_EXPIRATION));
        } else if (propertyKey == VIRTUAL_CARD_LABEL) {
            descriptionLabel.setText(model.get(VIRTUAL_CARD_LABEL));
        } else if (propertyKey == ON_CREDIT_CARD_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_CREDIT_CARD_CLICK_ACTION).run());
        } else if (propertyKey == ITEM_COLLECTION_INFO) {
            FillableItemCollectionInfo collectionInfo = model.get(ITEM_COLLECTION_INFO);
            if (collectionInfo != null) {
                descriptionLabel.setAccessibilityDelegate(
                        new TextViewCollectionInfoAccessibilityDelegate(collectionInfo));
            }
        } else if (propertyKey == IS_ACCEPTABLE) {
            if (model.get(IS_ACCEPTABLE)) {
                view.setEnabled(true);
                descriptionLabel.setMaxLines(1);
                cardName.setTextAppearance(R.style.TextAppearance_TextMedium_Primary);
                cardNumber.setTextAppearance(R.style.TextAppearance_TextMedium_Primary);
                icon.setAlpha(COMPLETE_OPACITY_ALPHA);
            } else {
                view.setEnabled(false);
                // When merchants have opted out of virtual cards, we convey it
                // via a message in description. Since this message is
                // important, we remove the max lines limit to avoid truncation.
                descriptionLabel.setMaxLines(Integer.MAX_VALUE);
                cardName.setTextAppearance(R.style.TextAppearance_TextMedium_Disabled);
                cardNumber.setTextAppearance(R.style.TextAppearance_TextMedium_Disabled);
                icon.setAlpha(GRAYED_OUT_OPACITY_ALPHA);
            }

        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    static void bindIbanItemView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == IBAN_VALUE) {
            TextView ibanValue = view.findViewById(R.id.iban_value);
            ibanValue.setText(model.get(IBAN_VALUE));
            ibanValue.setTextAppearance(R.style.TextAppearance_TextLarge_Primary);
        } else if (propertyKey == IBAN_NICKNAME) {
            TextView ibanNickname = view.findViewById(R.id.iban_nickname);
            if (!model.get(IBAN_NICKNAME).isEmpty()) {
                ibanNickname.setText(model.get(IBAN_NICKNAME));
                ibanNickname.setVisibility(View.VISIBLE);
            }
        } else if (propertyKey == ON_IBAN_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_IBAN_CLICK_ACTION).run());
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new header inside the ListView inside the TouchToFillPaymentMethodView.
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createHeaderItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_payment_method_header_item, parent, false);
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
            sheetHeaderImage.setImageDrawable(
                    AppCompatResources.getDrawable(
                            view.getContext(), model.get(IMAGE_DRAWABLE_ID)));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    static View createFillButtonView(ViewGroup parent) {
        View buttonView =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.touch_to_fill_fill_button, parent, false);
        AutofillUiUtils.setFilterTouchForSecurity(buttonView);
        return buttonView;
    }

    static void bindFillButtonView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ON_CREDIT_CARD_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_CREDIT_CARD_CLICK_ACTION).run());
            TextView buttonTitleText = view.findViewById(R.id.touch_to_fill_button_title);
            buttonTitleText.setText(R.string.autofill_payment_method_continue_button);
        } else if (propertyKey == ON_IBAN_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_IBAN_CLICK_ACTION).run());
            TextView buttonTitleText = view.findViewById(R.id.touch_to_fill_button_title);
            buttonTitleText.setText(R.string.autofill_payment_method_continue_button);
        } else if (propertyKey == CARD_IMAGE
                || propertyKey == NETWORK_NAME
                || propertyKey == CARD_NAME
                || propertyKey == CARD_NUMBER
                || propertyKey == CARD_EXPIRATION
                || propertyKey == VIRTUAL_CARD_LABEL
                || propertyKey == IBAN_VALUE
                || propertyKey == IBAN_NICKNAME
                || propertyKey == ITEM_COLLECTION_INFO
                || propertyKey == IS_ACCEPTABLE) {
            // Skip, because none of these changes affect the button
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new footer inside the ListView inside the TouchToFillPaymentMethodView.
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createFooterItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_payment_method_footer_item, parent, false);
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
        } else if (propertyKey == SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK) {
            setShowPaymentMethodsSettingsCallback(
                    view, model.get(SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK));
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

    private static void setShowPaymentMethodsSettingsCallback(View view, Runnable callback) {
        View managePaymentMethodsButton = view.findViewById(R.id.manage_payment_methods);
        managePaymentMethodsButton.setOnClickListener(unused -> callback.run());
    }
}

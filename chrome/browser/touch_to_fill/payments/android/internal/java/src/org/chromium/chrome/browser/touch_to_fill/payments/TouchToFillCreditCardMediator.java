// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.ON_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.SHOULD_SHOW_SCAN_CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.VISIBLE;

import android.content.Context;

import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.HeaderProperties;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the logic for the TouchToFillCreditCard component. It sets the state of the model and
 * reacts to events like clicks.
 */
class TouchToFillCreditCardMediator {
    // TODO(crbug/1383487): Remove the Context from the Mediator.
    private Context mContext;
    private TouchToFillCreditCardComponent.Delegate mDelegate;
    private PropertyModel mModel;

    void initialize(Context context, TouchToFillCreditCardComponent.Delegate delegate,
            PropertyModel model) {
        assert delegate != null;
        mContext = context;
        mDelegate = delegate;
        mModel = model;
    }

    void showSheet(CreditCard[] cards, boolean shouldShowScanCreditCard) {
        assert cards != null;

        ModelList sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();

        for (CreditCard card : cards) {
            final PropertyModel model = createCardModel(card);
            sheetItems.add(new ListItem(CREDIT_CARD, model));
        }

        if (cards.length == 1) {
            // Use the credit card model as the property model for the fill button too
            assert sheetItems.get(0).type == CREDIT_CARD;
            sheetItems.add(new ListItem(FILL_BUTTON, sheetItems.get(0).model));
        }

        sheetItems.add(0, buildHeader(hasOnlyLocalCards(cards)));

        mModel.set(VISIBLE, true);
        mModel.set(SHOULD_SHOW_SCAN_CREDIT_CARD, shouldShowScanCreditCard);
    }

    void hideSheet() {
        onDismissed(BottomSheetController.StateChangeReason.NONE);
    }

    public void onDismissed(@StateChangeReason int reason) {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        mDelegate.onDismissed();
    }

    public void scanCreditCard() {
        mDelegate.scanCreditCard();
    }

    public void showCreditCardSettings() {
        mDelegate.showCreditCardSettings();
    }

    public void onSelectedCreditCard(String uniqueId) {
        mDelegate.suggestionSelected(uniqueId);
    }

    private PropertyModel createCardModel(CreditCard card) {
        return new PropertyModel
                .Builder(TouchToFillCreditCardProperties.CreditCardProperties.ALL_KEYS)
                .with(TouchToFillCreditCardProperties.CreditCardProperties.CARD_ICON_ID,
                        card.getIssuerIconDrawableId())
                .with(TouchToFillCreditCardProperties.CreditCardProperties.CARD_NAME,
                        card.getCardNameForAutofillDisplay())
                .with(TouchToFillCreditCardProperties.CreditCardProperties.CARD_NUMBER,
                        card.getObfuscatedLastFourDigits())
                .with(TouchToFillCreditCardProperties.CreditCardProperties.CARD_EXPIRATION,
                        mContext.getString(
                                        R.string.autofill_credit_card_two_line_label_from_card_number)
                                .replace("$1", card.getFormattedExpirationDate(mContext)))
                .with(ON_CLICK_ACTION, () -> { this.onSelectedCreditCard(card.getGUID()); })
                .build();
    }

    private ListItem buildHeader(boolean hasOnlyLocalCards) {
        return new ListItem(TouchToFillCreditCardProperties.ItemType.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(IMAGE_DRAWABLE_ID,
                                hasOnlyLocalCards ? R.drawable.fre_product_logo
                                                  : R.drawable.google_pay)
                        .build());
    }

    private static boolean hasOnlyLocalCards(CreditCard[] cards) {
        for (CreditCard card : cards) {
            if (!card.getIsLocal()) return false;
        }
        return true;
    }
}

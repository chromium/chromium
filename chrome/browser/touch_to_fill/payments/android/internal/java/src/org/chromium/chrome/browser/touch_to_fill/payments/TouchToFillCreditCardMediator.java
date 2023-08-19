// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.ON_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.FooterProperties.SCAN_CREDIT_CARD_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.FooterProperties.SHOULD_SHOW_SCAN_CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.FooterProperties.SHOW_CREDIT_CARD_SETTINGS_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.VISIBLE;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.FooterProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.HeaderProperties;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

/**
 * Contains the logic for the TouchToFillCreditCard component. It sets the state of the model and
 * reacts to events like clicks.
 */
class TouchToFillCreditCardMediator {
    /**
     * The final outcome that closes the Touch To Fill sheet.
     *
     * Entries should not be renumbered and numeric values should never be reused. Needs to stay
     * in sync with TouchToFill.CreditCard.Outcome in enums.xml.
     */
    @IntDef({TouchToFillCreditCardOutcome.CREDIT_CARD, TouchToFillCreditCardOutcome.VIRTUAL_CARD,
            TouchToFillCreditCardOutcome.MANAGE_PAYMENTS,
            TouchToFillCreditCardOutcome.SCAN_NEW_CARD, TouchToFillCreditCardOutcome.DISMISS})
    @Retention(RetentionPolicy.SOURCE)
    @interface TouchToFillCreditCardOutcome {
        int CREDIT_CARD = 0;
        int VIRTUAL_CARD = 1;
        int MANAGE_PAYMENTS = 2;
        int SCAN_NEW_CARD = 3;
        int DISMISS = 4;
        int MAX_VALUE = DISMISS;
    }
    @VisibleForTesting
    static final String TOUCH_TO_FILL_OUTCOME_HISTOGRAM = "Autofill.TouchToFill.CreditCard.Outcome";
    @VisibleForTesting
    static final String TOUCH_TO_FILL_OUTCOME_HISTOGRAM_FIXED =
            "Autofill.TouchToFill.CreditCard.Outcome2";
    @VisibleForTesting
    static final String TOUCH_TO_FILL_INDEX_SELECTED =
            "Autofill.TouchToFill.CreditCard.SelectedIndex";
    @VisibleForTesting
    static final String TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN =
            "Autofill.TouchToFill.CreditCard.NumberOfCardsShown";

    // TODO(crbug/1383487): Remove the Context from the Mediator.
    private Context mContext;
    private TouchToFillCreditCardComponent.Delegate mDelegate;
    private PropertyModel mModel;
    private List<CreditCard> mCards;
    private BottomSheetFocusHelper mBottomSheetFocusHelper;

    void initialize(Context context, TouchToFillCreditCardComponent.Delegate delegate,
            PropertyModel model, BottomSheetFocusHelper bottomSheetFocusHelper) {
        assert delegate != null;
        mContext = context;
        mDelegate = delegate;
        mModel = model;
        mBottomSheetFocusHelper = bottomSheetFocusHelper;
    }

    void showSheet(CreditCard[] cards, boolean shouldShowScanCreditCard) {
        assert cards != null;
        mCards = Arrays.asList(cards);

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
        sheetItems.add(buildFooter(shouldShowScanCreditCard));

        mBottomSheetFocusHelper.registerForOneTimeUse();
        mModel.set(VISIBLE, true);

        RecordHistogram.recordCount100Histogram(TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, cards.length);
    }

    void hideSheet() {
        onDismissed(BottomSheetController.StateChangeReason.NONE);
    }

    public void onDismissed(@StateChangeReason int reason) {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        boolean dismissedByUser = reason == StateChangeReason.SWIPE
                || reason == StateChangeReason.BACK_PRESS || reason == StateChangeReason.TAP_SCRIM;
        mDelegate.onDismissed(dismissedByUser);
        // TODO (crbug.com/1247698) Record the old histogram as before to not mix the old and the
        // new metrics. Remove after M114 is launched.
        if (reason == StateChangeReason.SWIPE || reason == StateChangeReason.BACK_PRESS) {
            RecordHistogram.recordEnumeratedHistogram(TOUCH_TO_FILL_OUTCOME_HISTOGRAM,
                    TouchToFillCreditCardOutcome.DISMISS,
                    TouchToFillCreditCardOutcome.MAX_VALUE + 1);
        }
        if (dismissedByUser) {
            RecordHistogram.recordEnumeratedHistogram(TOUCH_TO_FILL_OUTCOME_HISTOGRAM_FIXED,
                    TouchToFillCreditCardOutcome.DISMISS,
                    TouchToFillCreditCardOutcome.MAX_VALUE + 1);
        }
    }

    public void scanCreditCard() {
        mDelegate.scanCreditCard();
        recordTouchToFillOutcomeHistogram(TouchToFillCreditCardOutcome.SCAN_NEW_CARD);
    }

    public void showCreditCardSettings() {
        mDelegate.showCreditCardSettings();
        recordTouchToFillOutcomeHistogram(TouchToFillCreditCardOutcome.MANAGE_PAYMENTS);
    }

    public void onSelectedCreditCard(CreditCard card) {
        mDelegate.suggestionSelected(card.getGUID(), card.getIsVirtual());
        recordTouchToFillOutcomeHistogram(card.getIsVirtual()
                        ? TouchToFillCreditCardOutcome.VIRTUAL_CARD
                        : TouchToFillCreditCardOutcome.CREDIT_CARD);
        RecordHistogram.recordCount100Histogram(TOUCH_TO_FILL_INDEX_SELECTED, mCards.indexOf(card));
    }

    private PropertyModel createCardModel(CreditCard card) {
        PropertyModel.Builder creditCardModelBuilder =
                new PropertyModel
                        .Builder(TouchToFillCreditCardProperties.CreditCardProperties.ALL_KEYS)
                        .with(TouchToFillCreditCardProperties.CreditCardProperties.CARD_ICON_ID,
                                card.getIssuerIconDrawableId())
                        .with(TouchToFillCreditCardProperties.CreditCardProperties.CARD_ART_URL,
                                AutofillUiUtils.shouldShowCustomIcon(
                                        card.getCardArtUrl(), card.getIsVirtual())
                                        ? card.getCardArtUrl()
                                        : new GURL(""))
                        .with(TouchToFillCreditCardProperties.CreditCardProperties.NETWORK_NAME, "")
                        .with(TouchToFillCreditCardProperties.CreditCardProperties.CARD_NAME,
                                card.getCardNameForAutofillDisplay())
                        .with(TouchToFillCreditCardProperties.CreditCardProperties.CARD_NUMBER,
                                card.getObfuscatedLastFourDigits())
                        .with(ON_CLICK_ACTION, () -> this.onSelectedCreditCard(card));

        // If a card has a nickname, the network name should also be announced, otherwise the name
        // of the card will be the network name and it will be announced.
        if (!card.getBasicCardIssuerNetwork().equals(
                    card.getCardNameForAutofillDisplay().toLowerCase(Locale.getDefault()))) {
            creditCardModelBuilder.with(
                    TouchToFillCreditCardProperties.CreditCardProperties.NETWORK_NAME,
                    card.getBasicCardIssuerNetwork());
        }

        // For virtual cards, show the "Virtual card" label on the second line, and for non-virtual
        // cards, show the expiration date.
        if (card.getIsVirtual()) {
            creditCardModelBuilder.with(
                    TouchToFillCreditCardProperties.CreditCardProperties.VIRTUAL_CARD_LABEL,
                    mContext.getString(R.string.autofill_virtual_card_number_switch_label));
        } else {
            creditCardModelBuilder.with(
                    TouchToFillCreditCardProperties.CreditCardProperties.CARD_EXPIRATION,
                    card.getFormattedExpirationDate(mContext));
        }
        return creditCardModelBuilder.build();
    }

    private ListItem buildHeader(boolean hasOnlyLocalCards) {
        return new ListItem(TouchToFillCreditCardProperties.ItemType.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(IMAGE_DRAWABLE_ID,
                                hasOnlyLocalCards ? R.drawable.fre_product_logo
                                                  : R.drawable.google_pay)
                        .build());
    }

    private ListItem buildFooter(boolean hasScanCardButton) {
        return new ListItem(TouchToFillCreditCardProperties.ItemType.FOOTER,
                new PropertyModel.Builder(FooterProperties.ALL_KEYS)
                        .with(SHOULD_SHOW_SCAN_CREDIT_CARD, hasScanCardButton)
                        .with(SCAN_CREDIT_CARD_CALLBACK, this::scanCreditCard)
                        .with(SHOW_CREDIT_CARD_SETTINGS_CALLBACK, this::showCreditCardSettings)
                        .build());
    }

    private static boolean hasOnlyLocalCards(CreditCard[] cards) {
        for (CreditCard card : cards) {
            if (!card.getIsLocal()) return false;
        }
        return true;
    }

    private static void recordTouchToFillOutcomeHistogram(
            @TouchToFillCreditCardOutcome int outcome) {
        RecordHistogram.recordEnumeratedHistogram(TOUCH_TO_FILL_OUTCOME_HISTOGRAM, outcome,
                TouchToFillCreditCardOutcome.MAX_VALUE + 1);
        RecordHistogram.recordEnumeratedHistogram(TOUCH_TO_FILL_OUTCOME_HISTOGRAM_FIXED, outcome,
                TouchToFillCreditCardOutcome.MAX_VALUE + 1);
    }
}

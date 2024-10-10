// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.APPLY_DEACTIVATED_STYLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.CARD_IMAGE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.FIRST_LINE_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.ITEM_COLLECTION_INFO;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MAIN_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MINOR_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.NETWORK_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.NON_TRANSFORMING_CREDIT_CARD_SUGGESTION_KEYS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.ON_CREDIT_CARD_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.SECOND_LINE_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SCAN_CREDIT_CARD_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SHOULD_SHOW_SCAN_CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_NICKNAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_VALUE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.NON_TRANSFORMING_IBAN_KEYS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.ON_IBAN_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.FOOTER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.HEADER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.IBAN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.TERMS_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TermsLabelProperties.CARD_BENEFITS_TERMS_AVAILABLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.VISIBLE;

import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodComponent.Delegate;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TermsLabelProperties;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.IbanRecordType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.payments.InputProtector;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Locale;
import java.util.function.Function;

/**
 * Contains the logic for the TouchToFillPaymentMethod component. It sets the state of the model and
 * reacts to events like clicks.
 */
class TouchToFillPaymentMethodMediator {
    /**
     * The final outcome that closes the credit card Touch To Fill sheet.
     *
     * <p>Entries should not be renumbered and numeric values should never be reused. Needs to stay
     * in sync with TouchToFill.CreditCard.Outcome in enums.xml.
     */
    @IntDef({
        TouchToFillCreditCardOutcome.CREDIT_CARD,
        TouchToFillCreditCardOutcome.VIRTUAL_CARD,
        TouchToFillCreditCardOutcome.MANAGE_PAYMENTS,
        TouchToFillCreditCardOutcome.SCAN_NEW_CARD,
        TouchToFillCreditCardOutcome.DISMISS
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface TouchToFillCreditCardOutcome {
        int CREDIT_CARD = 0;
        int VIRTUAL_CARD = 1;
        int MANAGE_PAYMENTS = 2;
        int SCAN_NEW_CARD = 3;
        int DISMISS = 4;
        int MAX_VALUE = DISMISS;
    }

    /**
     * The final outcome that closes the IBAN Touch To Fill sheet.
     *
     * <p>Entries should not be renumbered and numeric values should never be reused. Needs to stay
     * in sync with TouchToFill.Iban.Outcome in enums.xml.
     */
    @IntDef({
        TouchToFillIbanOutcome.IBAN,
        TouchToFillIbanOutcome.MANAGE_PAYMENTS,
        TouchToFillIbanOutcome.DISMISS
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface TouchToFillIbanOutcome {
        int IBAN = 0;
        int MANAGE_PAYMENTS = 1;
        int DISMISS = 2;
        int MAX_VALUE = DISMISS;
    }

    @VisibleForTesting
    static final String TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM =
            "Autofill.TouchToFill.CreditCard.Outcome2";

    @VisibleForTesting
    static final String TOUCH_TO_FILL_CREDIT_CARD_INDEX_SELECTED =
            "Autofill.TouchToFill.CreditCard.SelectedIndex";

    @VisibleForTesting
    static final String TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN =
            "Autofill.TouchToFill.CreditCard.NumberOfCardsShown";

    @VisibleForTesting
    static final String TOUCH_TO_FILL_IBAN_OUTCOME_HISTOGRAM = "Autofill.TouchToFill.Iban.Outcome";

    @VisibleForTesting
    static final String TOUCH_TO_FILL_IBAN_INDEX_SELECTED =
            "Autofill.TouchToFill.Iban.SelectedIndex";

    @VisibleForTesting
    static final String TOUCH_TO_FILL_NUMBER_OF_IBANS_SHOWN =
            "Autofill.TouchToFill.Iban.NumberOfIbansShown";

    private TouchToFillPaymentMethodComponent.Delegate mDelegate;
    private PropertyModel mModel;
    private List<CreditCard> mCards;
    private List<Iban> mIbans;
    private BottomSheetFocusHelper mBottomSheetFocusHelper;

    private InputProtector mInputProtector = new InputProtector();

    void initialize(
            Delegate delegate, PropertyModel model, BottomSheetFocusHelper bottomSheetFocusHelper) {
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
        mBottomSheetFocusHelper = bottomSheetFocusHelper;
    }

    void showSheet(
            List<CreditCard> cards,
            List<AutofillSuggestion> suggestions,
            boolean shouldShowScanCreditCard,
            Function<TouchToFillPaymentMethodProperties.CardImageMetaData, Drawable>
                    cardImageFunction) {
        mInputProtector.markShowTime();

        assert cards != null;
        mCards = cards;
        mIbans = null;
        assert mCards.size() == suggestions.size()
                : "The number of cards and suggestions should be same.";

        ModelList sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();
        boolean cardBenefitsTermsAvailable = false;

        for (int i = 0; i < mCards.size(); ++i) {
            CreditCard card = mCards.get(i);
            final PropertyModel model =
                    createCardSuggestionModel(
                            card,
                            suggestions.get(i),
                            new FillableItemCollectionInfo(i + 1, mCards.size()),
                            cardImageFunction);
            sheetItems.add(new ListItem(CREDIT_CARD, model));
            cardBenefitsTermsAvailable |= suggestions.get(i).shouldDisplayTermsAvailable();
        }

        if (cardBenefitsTermsAvailable) {
            sheetItems.add(buildTermsLabel(cardBenefitsTermsAvailable));
        }

        if (mCards.size() == 1) {
            // Use the credit card model as the property model for the fill button too
            assert sheetItems.get(0).type == CREDIT_CARD;
            sheetItems.add(new ListItem(FILL_BUTTON, sheetItems.get(0).model));
        }

        sheetItems.add(0, buildHeader(hasOnlyLocalCards(mCards)));
        sheetItems.add(buildFooterForCreditCard(shouldShowScanCreditCard));

        mBottomSheetFocusHelper.registerForOneTimeUse();
        mModel.set(VISIBLE, true);

        RecordHistogram.recordCount100Histogram(TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, mCards.size());
    }

    public void showSheet(List<Iban> ibans) {
        mInputProtector.markShowTime();

        assert ibans != null;
        mIbans = ibans;
        mCards = null;

        ModelList sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();

        for (int i = 0; i < mIbans.size(); ++i) {
            Iban iban = mIbans.get(i);
            final PropertyModel model = createIbanModel(iban);
            sheetItems.add(new ListItem(IBAN, model));
        }

        if (mIbans.size() == 1) {
            // Use the IBAN model as the property model for the fill button too.
            assert sheetItems.get(0).type == IBAN;
            sheetItems.add(new ListItem(FILL_BUTTON, sheetItems.get(0).model));
        }

        sheetItems.add(0, buildHeader(/* hasOnlyLocalPaymentMethods= */ true));
        sheetItems.add(buildFooterForIban());

        mBottomSheetFocusHelper.registerForOneTimeUse();
        mModel.set(VISIBLE, true);

        RecordHistogram.recordCount100Histogram(TOUCH_TO_FILL_NUMBER_OF_IBANS_SHOWN, mIbans.size());
    }

    void hideSheet() {
        onDismissed(BottomSheetController.StateChangeReason.NONE);
    }

    public void onDismissed(@StateChangeReason int reason) {
        // TODO(b/332193789): Add IBAN-related metrics.
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        boolean dismissedByUser =
                reason == StateChangeReason.SWIPE
                        || reason == StateChangeReason.BACK_PRESS
                        || reason == StateChangeReason.TAP_SCRIM;
        mDelegate.onDismissed(dismissedByUser);
        if (dismissedByUser) {
            if (mCards != null) {
                RecordHistogram.recordEnumeratedHistogram(
                        TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.DISMISS,
                        TouchToFillCreditCardOutcome.MAX_VALUE + 1);
            } else {
                assert mIbans != null;
                RecordHistogram.recordEnumeratedHistogram(
                        TOUCH_TO_FILL_IBAN_OUTCOME_HISTOGRAM,
                        TouchToFillIbanOutcome.DISMISS,
                        TouchToFillIbanOutcome.MAX_VALUE + 1);
            }
        }
    }

    public void scanCreditCard() {
        mDelegate.scanCreditCard();
        recordTouchToFillCreditCardOutcomeHistogram(TouchToFillCreditCardOutcome.SCAN_NEW_CARD);
    }

    public void showPaymentMethodSettings() {
        mDelegate.showPaymentMethodSettings();
        if (mCards != null) {
            recordTouchToFillCreditCardOutcomeHistogram(
                    TouchToFillCreditCardOutcome.MANAGE_PAYMENTS);
        } else {
            assert mIbans != null;
            recordTouchToFillIbanOutcomeHistogram(TouchToFillIbanOutcome.MANAGE_PAYMENTS);
        }
    }

    public void onSelectedCreditCard(CreditCard card) {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        mDelegate.creditCardSuggestionSelected(card.getGUID(), card.getIsVirtual());
        recordTouchToFillCreditCardOutcomeHistogram(
                card.getIsVirtual()
                        ? TouchToFillCreditCardOutcome.VIRTUAL_CARD
                        : TouchToFillCreditCardOutcome.CREDIT_CARD);
        RecordHistogram.recordCount100Histogram(
                TOUCH_TO_FILL_CREDIT_CARD_INDEX_SELECTED, mCards.indexOf(card));
    }

    public void onSelectedIban(Iban iban) {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        if (iban.getRecordType() == IbanRecordType.LOCAL_IBAN) {
            mDelegate.localIbanSuggestionSelected(iban.getGuid());
        } else {
            mDelegate.serverIbanSuggestionSelected(iban.getInstrumentId());
        }
        recordTouchToFillIbanOutcomeHistogram(TouchToFillIbanOutcome.IBAN);
        RecordHistogram.recordCount100Histogram(
                TOUCH_TO_FILL_IBAN_INDEX_SELECTED, mIbans.indexOf(iban));
    }

    private PropertyModel createCardSuggestionModel(
            CreditCard card,
            AutofillSuggestion suggestion,
            FillableItemCollectionInfo itemCollectionInfo,
            Function<TouchToFillPaymentMethodProperties.CardImageMetaData, Drawable>
                    cardImageFunction) {
        int drawableId = card.getIssuerIconDrawableId();
        GURL artUrl =
                AutofillUiUtils.shouldShowCustomIcon(card.getCardArtUrl(), card.getIsVirtual())
                        ? card.getCardArtUrl()
                        : new GURL("");
        TouchToFillPaymentMethodProperties.CardImageMetaData cardImageMetaData =
                new TouchToFillPaymentMethodProperties.CardImageMetaData(drawableId, artUrl);
        PropertyModel.Builder creditCardSuggestionModelBuilder =
                new PropertyModel.Builder(NON_TRANSFORMING_CREDIT_CARD_SUGGESTION_KEYS)
                        .withTransformingKey(CARD_IMAGE, cardImageFunction, cardImageMetaData)
                        .with(NETWORK_NAME, "")
                        .with(MAIN_TEXT, suggestion.getLabel())
                        .with(MINOR_TEXT, suggestion.getSecondaryLabel())
                        // For virtual cards, show the "Virtual card" label on the second
                        // line, and for non-virtual cards, show the expiration date.
                        // If the merchant has opted-out for the virtual card, on the second
                        // line we convey that merchant does not accept this virtual card.
                        // For cards with benefits, show the benefits on the second line and
                        // the expiration date or virtual card status on the third line.
                        .with(FIRST_LINE_LABEL, suggestion.getSublabel())
                        .with(SECOND_LINE_LABEL, suggestion.getSecondarySublabel())
                        .with(ON_CREDIT_CARD_CLICK_ACTION, () -> this.onSelectedCreditCard(card))
                        .with(ITEM_COLLECTION_INFO, itemCollectionInfo)
                        .with(APPLY_DEACTIVATED_STYLE, suggestion.applyDeactivatedStyle());

        // If a card has a nickname, the network name should also be announced, otherwise the name
        // of the card will be the network name and it will be announced.
        if (!card.getBasicCardIssuerNetwork()
                .equals(card.getCardNameForAutofillDisplay().toLowerCase(Locale.getDefault()))) {
            creditCardSuggestionModelBuilder.with(NETWORK_NAME, card.getBasicCardIssuerNetwork());
        }

        return creditCardSuggestionModelBuilder.build();
    }

    private PropertyModel createIbanModel(Iban iban) {
        PropertyModel.Builder ibanModelBuilder =
                new PropertyModel.Builder(NON_TRANSFORMING_IBAN_KEYS)
                        .with(IBAN_VALUE, iban.getLabel())
                        .with(IBAN_NICKNAME, iban.getNickname())
                        .with(ON_IBAN_CLICK_ACTION, () -> this.onSelectedIban(iban));
        return ibanModelBuilder.build();
    }

    private ListItem buildTermsLabel(boolean cardBenefitsTermsAvailable) {
        return new ListItem(
                TERMS_LABEL,
                new PropertyModel.Builder(TermsLabelProperties.ALL_TERMS_LABEL_KEYS)
                        .with(CARD_BENEFITS_TERMS_AVAILABLE, cardBenefitsTermsAvailable)
                        .build());
    }

    private ListItem buildHeader(boolean hasOnlyLocalPaymentMethods) {
        return new ListItem(
                HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(
                                IMAGE_DRAWABLE_ID,
                                hasOnlyLocalPaymentMethods
                                        ? R.drawable.fre_product_logo
                                        : R.drawable.google_pay)
                        .build());
    }

    private ListItem buildFooterForCreditCard(boolean hasScanCardButton) {
        return new ListItem(
                FOOTER,
                new PropertyModel.Builder(FooterProperties.ALL_KEYS)
                        .with(SHOULD_SHOW_SCAN_CREDIT_CARD, hasScanCardButton)
                        .with(SCAN_CREDIT_CARD_CALLBACK, this::scanCreditCard)
                        .with(
                                SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK,
                                this::showPaymentMethodSettings)
                        .build());
    }

    private ListItem buildFooterForIban() {
        return new ListItem(
                FOOTER,
                new PropertyModel.Builder(FooterProperties.ALL_KEYS)
                        .with(
                                SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK,
                                this::showPaymentMethodSettings)
                        .build());
    }

    private static boolean hasOnlyLocalCards(List<CreditCard> cards) {
        for (CreditCard card : cards) {
            if (!card.getIsLocal()) return false;
        }
        return true;
    }

    private static void recordTouchToFillCreditCardOutcomeHistogram(
            @TouchToFillCreditCardOutcome int outcome) {
        RecordHistogram.recordEnumeratedHistogram(
                TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                outcome,
                TouchToFillCreditCardOutcome.MAX_VALUE + 1);
    }

    private static void recordTouchToFillIbanOutcomeHistogram(@TouchToFillIbanOutcome int outcome) {
        RecordHistogram.recordEnumeratedHistogram(
                TOUCH_TO_FILL_IBAN_OUTCOME_HISTOGRAM,
                outcome,
                TouchToFillIbanOutcome.MAX_VALUE + 1);
    }

    void setInputProtectorForTesting(InputProtector inputProtector) {
        mInputProtector = inputProtector;
    }
}

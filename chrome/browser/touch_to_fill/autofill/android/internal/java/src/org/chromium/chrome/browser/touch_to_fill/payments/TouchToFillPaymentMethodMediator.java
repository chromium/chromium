// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.APPLY_DEACTIVATED_STYLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.CARD_IMAGE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.FIRST_LINE_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.ITEM_COLLECTION_INFO;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MAIN_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MAIN_TEXT_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MINOR_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.NON_TRANSFORMING_CREDIT_CARD_SUGGESTION_KEYS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.ON_CREDIT_CARD_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.SECOND_LINE_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SCAN_CREDIT_CARD_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SHOULD_SHOW_SCAN_CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.TITLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_NICKNAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_VALUE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.NON_TRANSFORMING_IBAN_KEYS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.ON_IBAN_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.FOOTER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.HEADER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.IBAN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.LOYALTY_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.TERMS_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.LOYALTY_CARD_NUMBER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.MERCHANT_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.NON_TRANSFORMING_LOYALTY_CARD_KEYS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.ON_LOYALTY_CARD_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TermsLabelProperties.CARD_BENEFITS_TERMS_AVAILABLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.VISIBLE;

import android.graphics.drawable.Drawable;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
import org.chromium.chrome.browser.touch_to_fill.common.TouchToFillResourceProvider;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodComponent.Delegate;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TermsLabelProperties;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.IbanRecordType;
import org.chromium.components.autofill.LoyaltyCard;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.payments.ui.InputProtector;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
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
    private List<AutofillSuggestion> mSuggestions;
    private List<Iban> mIbans;
    private List<LoyaltyCard> mLoyaltyCards;
    private BottomSheetFocusHelper mBottomSheetFocusHelper;

    private InputProtector mInputProtector = new InputProtector();

    void initialize(
            Delegate delegate, PropertyModel model, BottomSheetFocusHelper bottomSheetFocusHelper) {
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
        mBottomSheetFocusHelper = bottomSheetFocusHelper;
    }

    void showCreditCards(
            List<AutofillSuggestion> suggestions,
            boolean shouldShowScanCreditCard,
            Function<TouchToFillPaymentMethodProperties.CardImageMetaData, Drawable>
                    cardImageFunction) {
        mInputProtector.markShowTime();

        assert suggestions != null;
        mSuggestions = suggestions;
        mIbans = null;
        mLoyaltyCards = null;

        ModelList sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();
        boolean cardBenefitsTermsAvailable = false;

        for (int i = 0; i < mSuggestions.size(); ++i) {
            AutofillSuggestion suggestion = mSuggestions.get(i);
            final PropertyModel model =
                    createCardSuggestionModel(
                            suggestion,
                            new FillableItemCollectionInfo(i + 1, mSuggestions.size()),
                            cardImageFunction);
            sheetItems.add(new ListItem(CREDIT_CARD, model));
            cardBenefitsTermsAvailable |= suggestion.shouldDisplayTermsAvailable();
        }

        if (cardBenefitsTermsAvailable) {
            sheetItems.add(buildTermsLabel(cardBenefitsTermsAvailable));
        }

        if (mSuggestions.size() == 1) {
            // Use the credit card model as the property model for the fill button too
            assert sheetItems.get(0).type == CREDIT_CARD;
            sheetItems.add(new ListItem(FILL_BUTTON, sheetItems.get(0).model));
        }

        sheetItems.add(0, buildHeaderForPayments(hasOnlyLocalCards(mSuggestions)));
        sheetItems.add(buildFooterForCreditCard(shouldShowScanCreditCard));

        mBottomSheetFocusHelper.registerForOneTimeUse();
        mModel.set(VISIBLE, true);

        RecordHistogram.recordCount100Histogram(
                TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, mSuggestions.size());
    }

    public void showIbans(List<Iban> ibans) {
        mInputProtector.markShowTime();

        assert ibans != null;
        mIbans = ibans;
        mSuggestions = null;
        mLoyaltyCards = null;

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

        sheetItems.add(0, buildHeaderForPayments(/* hasOnlyLocalPaymentMethods= */ true));
        sheetItems.add(buildFooterForIban());

        mBottomSheetFocusHelper.registerForOneTimeUse();
        mModel.set(VISIBLE, true);

        RecordHistogram.recordCount100Histogram(TOUCH_TO_FILL_NUMBER_OF_IBANS_SHOWN, mIbans.size());
    }

    public void showLoyaltyCards(List<LoyaltyCard> loyaltyCards) {
        mInputProtector.markShowTime();

        assert loyaltyCards != null;
        mLoyaltyCards = loyaltyCards;
        mSuggestions = null;
        mIbans = null;

        ModelList sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();

        for (LoyaltyCard loyaltyCard : mLoyaltyCards) {
            final PropertyModel model = createLoyaltyCardModel(loyaltyCard);
            sheetItems.add(new ListItem(LOYALTY_CARD, model));
        }

        if (mLoyaltyCards.size() == 1) {
            // Use the LOYALTY_CARD model as the property model for the fill button too.
            assert sheetItems.get(0).type == LOYALTY_CARD;
            sheetItems.add(new ListItem(FILL_BUTTON, sheetItems.get(0).model));
        }

        sheetItems.add(0, buildHeaderForLoyaltyCards());

        mBottomSheetFocusHelper.registerForOneTimeUse();
        mModel.set(VISIBLE, true);

        // TODO: crbug.com/404437211 - Log the number of loyalty cards shown.
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
            if (mSuggestions != null) {
                RecordHistogram.recordEnumeratedHistogram(
                        TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.DISMISS,
                        TouchToFillCreditCardOutcome.MAX_VALUE);
            } else if (mIbans != null) {
                RecordHistogram.recordEnumeratedHistogram(
                        TOUCH_TO_FILL_IBAN_OUTCOME_HISTOGRAM,
                        TouchToFillIbanOutcome.DISMISS,
                        TouchToFillIbanOutcome.MAX_VALUE);
            } else {
                assert mLoyaltyCards != null;
                // TODO: crbug.com/404437211 - Log loyalty card metrics.
            }
        }
    }

    public void scanCreditCard() {
        mDelegate.scanCreditCard();
        recordTouchToFillCreditCardOutcomeHistogram(TouchToFillCreditCardOutcome.SCAN_NEW_CARD);
    }

    public void showPaymentMethodSettings() {
        mDelegate.showPaymentMethodSettings();
        if (mSuggestions != null) {
            recordTouchToFillCreditCardOutcomeHistogram(
                    TouchToFillCreditCardOutcome.MANAGE_PAYMENTS);
        } else {
            assert mIbans != null;
            recordTouchToFillIbanOutcomeHistogram(TouchToFillIbanOutcome.MANAGE_PAYMENTS);
        }
    }

    private void onSelectedCreditCard(AutofillSuggestion suggestion) {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        boolean is_virtual_card =
                suggestion.getSuggestionType() == SuggestionType.VIRTUAL_CREDIT_CARD_ENTRY;
        mDelegate.creditCardSuggestionSelected(suggestion.getGuid(), is_virtual_card);
        recordTouchToFillCreditCardOutcomeHistogram(
                is_virtual_card
                        ? TouchToFillCreditCardOutcome.VIRTUAL_CARD
                        : TouchToFillCreditCardOutcome.CREDIT_CARD);
        RecordHistogram.recordCount100Histogram(
                TOUCH_TO_FILL_CREDIT_CARD_INDEX_SELECTED, mSuggestions.indexOf(suggestion));
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

    public void onSelectedLoyaltyCard(LoyaltyCard loyaltyCard) {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        mDelegate.loyaltyCardSuggestionSelected(loyaltyCard.getLoyaltyCardNumber());
    }

    private PropertyModel createCardSuggestionModel(
            AutofillSuggestion suggestion,
            FillableItemCollectionInfo itemCollectionInfo,
            Function<TouchToFillPaymentMethodProperties.CardImageMetaData, Drawable>
                    cardImageFunction) {
        int drawableId = suggestion.getIconId();
        GURL artUrl =
                AutofillUiUtils.shouldShowCustomIcon(
                                suggestion.getCustomIconUrl(),
                                suggestion.getSuggestionType()
                                        == SuggestionType.VIRTUAL_CREDIT_CARD_ENTRY)
                        ? suggestion.getCustomIconUrl()
                        : new GURL("");
        TouchToFillPaymentMethodProperties.CardImageMetaData cardImageMetaData =
                new TouchToFillPaymentMethodProperties.CardImageMetaData(drawableId, artUrl);
        PropertyModel.Builder creditCardSuggestionModelBuilder =
                new PropertyModel.Builder(NON_TRANSFORMING_CREDIT_CARD_SUGGESTION_KEYS)
                        .withTransformingKey(CARD_IMAGE, cardImageFunction, cardImageMetaData)
                        .with(MAIN_TEXT, suggestion.getLabel())
                        .with(
                                MAIN_TEXT_CONTENT_DESCRIPTION,
                                suggestion.getLabelContentDescription())
                        .with(MINOR_TEXT, suggestion.getSecondaryLabel())
                        // For virtual cards, show the "Virtual card" label on the second
                        // line, and for non-virtual cards, show the expiration date.
                        // If the merchant has opted-out for the virtual card, on the second
                        // line we convey that merchant does not accept this virtual card.
                        // For cards with benefits, show the benefits on the second line and
                        // the expiration date or virtual card status on the third line.
                        .with(FIRST_LINE_LABEL, suggestion.getSublabel())
                        .with(SECOND_LINE_LABEL, suggestion.getSecondarySublabel())
                        .with(
                                ON_CREDIT_CARD_CLICK_ACTION,
                                () -> this.onSelectedCreditCard(suggestion))
                        .with(ITEM_COLLECTION_INFO, itemCollectionInfo)
                        .with(APPLY_DEACTIVATED_STYLE, suggestion.applyDeactivatedStyle());
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

    private PropertyModel createLoyaltyCardModel(LoyaltyCard loyaltyCard) {
        PropertyModel.Builder loyaltyCardModelBuilder =
                new PropertyModel.Builder(NON_TRANSFORMING_LOYALTY_CARD_KEYS)
                        .with(LOYALTY_CARD_NUMBER, loyaltyCard.getLoyaltyCardNumber())
                        .with(MERCHANT_NAME, loyaltyCard.getMerchantName())
                        .with(
                                ON_LOYALTY_CARD_CLICK_ACTION,
                                () -> this.onSelectedLoyaltyCard(loyaltyCard));

        return loyaltyCardModelBuilder.build();
    }

    private ListItem buildTermsLabel(boolean cardBenefitsTermsAvailable) {
        return new ListItem(
                TERMS_LABEL,
                new PropertyModel.Builder(TermsLabelProperties.ALL_TERMS_LABEL_KEYS)
                        .with(CARD_BENEFITS_TERMS_AVAILABLE, cardBenefitsTermsAvailable)
                        .build());
    }

    private ListItem buildHeaderForPayments(boolean hasOnlyLocalPaymentMethods) {
        return new ListItem(
                HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(
                                IMAGE_DRAWABLE_ID,
                                hasOnlyLocalPaymentMethods
                                        ? R.drawable.fre_product_logo
                                        : R.drawable.google_pay)
                        .with(TITLE_ID, R.string.autofill_payment_method_bottom_sheet_title)
                        .build());
    }

    private ListItem buildHeaderForLoyaltyCards() {
        @Nullable
        final TouchToFillResourceProvider mResourceProvider =
                ServiceLoaderUtil.maybeCreate(TouchToFillResourceProvider.class);
        @DrawableRes
        final int headerImageId =
                mResourceProvider == null
                        ? R.drawable.touch_to_fill_default_header_image
                        : mResourceProvider.getLoyaltyCardHeaderDrawableId();
        return new ListItem(
                HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(IMAGE_DRAWABLE_ID, headerImageId)
                        .with(TITLE_ID, R.string.autofill_loyalty_card_bottom_sheet_title)
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

    private static boolean hasOnlyLocalCards(List<AutofillSuggestion> suggestions) {
        for (AutofillSuggestion suggestion : suggestions) {
            if (!suggestion.isLocalPaymentsMethod()) return false;
        }
        return true;
    }

    private static void recordTouchToFillCreditCardOutcomeHistogram(
            @TouchToFillCreditCardOutcome int outcome) {
        RecordHistogram.recordEnumeratedHistogram(
                TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                outcome,
                TouchToFillCreditCardOutcome.MAX_VALUE);
    }

    private static void recordTouchToFillIbanOutcomeHistogram(@TouchToFillIbanOutcome int outcome) {
        RecordHistogram.recordEnumeratedHistogram(
                TOUCH_TO_FILL_IBAN_OUTCOME_HISTOGRAM,
                outcome,
                TouchToFillIbanOutcome.MAX_VALUE);
    }

    void setInputProtectorForTesting(InputProtector inputProtector) {
        mInputProtector = inputProtector;
    }
}

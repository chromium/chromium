// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.APPLY_ISSUER_DEACTIVATED_STYLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_ICON_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_LINKED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_SELECTION_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.NON_TRANSFORMING_BNPL_ISSUER_CONTEXT_KEYS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ON_ISSUER_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressFooterProperties.APPLY_LINK_DEACTIVATED_STYLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressFooterProperties.HIDE_OPTIONS_LINK_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressFooterProperties.ON_LINK_CLICK_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressFooterProperties.TERMS_TEXT_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.BNPL_ICON_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.BNPL_ITEM_COLLECTION_INFO;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.IS_ENABLED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.NON_TRANSFORMING_BNPL_SUGGESTION_KEYS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.ON_BNPL_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.PRIMARY_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.SECONDARY_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ButtonProperties.ON_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ButtonProperties.TEXT_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CURRENT_SCREEN;
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
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ErrorDescriptionProperties.ERROR_DESCRIPTION_STRING;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FOCUSED_VIEW_ID_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.OPEN_MANAGEMENT_UI_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.OPEN_MANAGEMENT_UI_TITLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SCAN_CREDIT_CARD_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SHOULD_SHOW_SCAN_CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.SUBTITLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.TITLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.TITLE_STRING;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_NICKNAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_VALUE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.NON_TRANSFORMING_IBAN_KEYS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.ON_IBAN_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.ALL_LOYALTY_CARDS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL_ISSUER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL_SELECTION_PROGRESS_FOOTER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL_SELECTION_PROGRESS_HEADER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL_TOS_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.ERROR_DESCRIPTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.FOOTER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.HEADER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.IBAN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.LOYALTY_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.PROGRESS_ICON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.TERMS_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.TEXT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.TOS_FOOTER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.WALLET_SETTINGS_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.LOYALTY_CARD_ICON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.LOYALTY_CARD_NUMBER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.MERCHANT_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.NON_TRANSFORMING_LOYALTY_CARD_KEYS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.ON_LOYALTY_CARD_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ProgressIconProperties.PROGRESS_CONTENT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_CLOSED_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_CONTENT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_FULL_HEIGHT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_HALF_HEIGHT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.ALL_LOYALTY_CARDS_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.BNPL_ISSUER_SELECTION_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.BNPL_ISSUER_TOS_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.ERROR_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.HOME_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.PROGRESS_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TermsLabelProperties.TERMS_LABEL_TEXT_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TosFooterProperties.LEGAL_MESSAGE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.VISIBLE;

import android.graphics.drawable.Drawable;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.night_mode.GlobalNightModeStateProviderHolder;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
import org.chromium.chrome.browser.touch_to_fill.common.TouchToFillResourceProvider;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodComponent.Delegate;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.AllLoyaltyCardsItemProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerTosTextItemProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressFooterProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressHeaderProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ButtonProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ErrorDescriptionProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ProgressIconProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TermsLabelProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TosFooterProperties;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.IbanRecordType;
import org.chromium.components.autofill.LoyaltyCard;
import org.chromium.components.autofill.PaymentsPayload;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.autofill.payments.BnplIssuerContext;
import org.chromium.components.autofill.payments.BnplIssuerTosDetail;
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

    /**
     * The final outcome that closes the loyalty card Touch To Fill sheet.
     *
     * <p>Entries should not be renumbered and numeric values should never be reused. Needs to stay
     * in sync with TouchToFill.LoyaltyCard.Outcome in enums.xml.
     */
    @IntDef({
        TouchToFillLoyaltyCardOutcome.AFFILIATED_LOYALTY_CARD,
        TouchToFillLoyaltyCardOutcome.NON_AFFILIATED_LOYALTY_CARD,
        TouchToFillLoyaltyCardOutcome.WALLET_SETTINGS,
        TouchToFillLoyaltyCardOutcome.MANAGE_LOYALTY_CARDS,
        TouchToFillLoyaltyCardOutcome.DISMISS
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface TouchToFillLoyaltyCardOutcome {
        int AFFILIATED_LOYALTY_CARD = 0;
        int NON_AFFILIATED_LOYALTY_CARD = 1;
        int WALLET_SETTINGS = 2;
        int MANAGE_LOYALTY_CARDS = 3;
        int DISMISS = 4;
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

    @VisibleForTesting
    static final String TOUCH_TO_FILL_LOYALTY_CARD_OUTCOME_HISTOGRAM =
            "Autofill.TouchToFill.LoyaltyCard.Outcome";

    @VisibleForTesting
    static final String TOUCH_TO_FILL_AFFILIATED_LOYALTY_CARDS_SCREEN_INDEX_SELECTED =
            "Autofill.TouchToFill.LoyaltyCard.Affiliated.SelectedIndex";

    @VisibleForTesting
    static final String TOUCH_TO_FILL_ALL_LOYALTY_CARDS_SCREEN_INDEX_SELECTED =
            "Autofill.TouchToFill.LoyaltyCard.All.SelectedIndex";

    @VisibleForTesting
    static final String TOUCH_TO_FILL_NUMBER_OF_LOYALTY_CARDS_SHOWN =
            "Autofill.TouchToFill.LoyaltyCard.NumberOfLoyaltyCardsShown";

    @VisibleForTesting
    static final String TOUCH_TO_FILL_NUMBER_OF_AFFILIATED_LOYALTY_CARDS_SHOWN =
            "Autofill.TouchToFill.LoyaltyCard.NumberOfAffiliatedLoyaltyCardsShown";

    private TouchToFillPaymentMethodComponent.Delegate mDelegate;
    private PropertyModel mModel;
    private List<AutofillSuggestion> mSuggestions;
    private List<Iban> mIbans;
    private List<LoyaltyCard> mAffiliatedLoyaltyCards;
    private List<LoyaltyCard> mAllLoyaltyCards;
    private List<BnplIssuerContext> mBnplIssuerContexts;
    private Function<LoyaltyCard, Drawable> mValuableImageFunction;
    private BottomSheetFocusHelper mBottomSheetFocusHelper;
    private boolean mShouldShowScanCreditCard;
    private Function<TouchToFillPaymentMethodProperties.CardImageMetaData, Drawable>
            mCardImageFunction;

    private InputProtector mInputProtector = new InputProtector();

    void initialize(
            Delegate delegate, PropertyModel model, BottomSheetFocusHelper bottomSheetFocusHelper) {
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
        mBottomSheetFocusHelper = bottomSheetFocusHelper;
    }

    void showPaymentMethods(
            List<AutofillSuggestion> suggestions,
            boolean shouldShowScanCreditCard,
            Function<TouchToFillPaymentMethodProperties.CardImageMetaData, Drawable>
                    cardImageFunction) {
        mInputProtector.markShowTime();

        assert suggestions != null;
        mSuggestions = suggestions;
        mShouldShowScanCreditCard = shouldShowScanCreditCard;
        mCardImageFunction = cardImageFunction;
        mIbans = null;
        mAffiliatedLoyaltyCards = null;
        mAllLoyaltyCards = null;
        mValuableImageFunction = null;
        mBnplIssuerContexts = null;

        mBottomSheetFocusHelper.registerForOneTimeUse();

        setPaymentMethodsHomeScreenItems();

        RecordHistogram.recordCount100Histogram(
                TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, mSuggestions.size());
    }

    private void setPaymentMethodsHomeScreenItems() {
        assert mSuggestions != null;

        ModelList sheetItems = new ModelList();
        boolean cardBenefitsTermsAvailable = false;

        for (int i = 0; i < mSuggestions.size(); ++i) {
            AutofillSuggestion suggestion = mSuggestions.get(i);
            if (suggestion.getSuggestionType() == SuggestionType.BNPL_ENTRY) {
                sheetItems.add(
                        new ListItem(
                                BNPL,
                                createBnplSuggestionModel(
                                        suggestion,
                                        new FillableItemCollectionInfo(
                                                i + 1, mSuggestions.size()))));
            } else {
                sheetItems.add(
                        new ListItem(
                                CREDIT_CARD,
                                createCardSuggestionModel(
                                        suggestion,
                                        new FillableItemCollectionInfo(i + 1, mSuggestions.size()),
                                        mCardImageFunction)));
            }
            PaymentsPayload payload = suggestion.getPaymentsPayload();
            if (payload != null) {
                cardBenefitsTermsAvailable |= payload.shouldDisplayTermsAvailable();
            }
        }

        if (cardBenefitsTermsAvailable) {
            sheetItems.add(buildCardBenefitTermsLabel());
        }

        if (mSuggestions.size() == 1) {
            // Use the credit card model as the property model for the fill button too
            assert sheetItems.get(0).type == CREDIT_CARD;
            sheetItems.add(
                    new ListItem(
                            FILL_BUTTON,
                            createButtonModel(
                                    R.string.autofill_payment_method_continue_button,
                                    () -> onSelectedCreditCard(mSuggestions.get(0)))));
        }

        sheetItems.add(0, buildHeaderForPayments(hasOnlyLocalCards(mSuggestions)));
        sheetItems.add(buildFooterForCreditCard(mShouldShowScanCreditCard));

        mModel.set(SHEET_ITEMS, sheetItems);
        mModel.set(
                SHEET_CONTENT_DESCRIPTION_ID,
                R.string.autofill_payment_method_bottom_sheet_content_description);
        mModel.set(
                SHEET_HALF_HEIGHT_DESCRIPTION_ID,
                R.string.autofill_payment_method_bottom_sheet_half_height);
        mModel.set(
                SHEET_FULL_HEIGHT_DESCRIPTION_ID,
                R.string.autofill_payment_method_bottom_sheet_full_height);
        mModel.set(
                SHEET_CLOSED_DESCRIPTION_ID, R.string.autofill_payment_method_bottom_sheet_closed);
        mModel.set(VISIBLE, true);
    }

    public void showIbans(List<Iban> ibans) {
        mInputProtector.markShowTime();

        assert ibans != null;
        mIbans = ibans;
        mSuggestions = null;
        mAffiliatedLoyaltyCards = null;
        mAllLoyaltyCards = null;
        mValuableImageFunction = null;
        mShouldShowScanCreditCard = false;
        mCardImageFunction = null;
        mBnplIssuerContexts = null;

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
            sheetItems.add(
                    new ListItem(
                            FILL_BUTTON,
                            createButtonModel(
                                    R.string.autofill_payment_method_continue_button,
                                    () -> this.onSelectedIban(mIbans.get(0)))));
        }

        sheetItems.add(0, buildHeaderForPayments(/* hasOnlyLocalPaymentMethods= */ true));
        sheetItems.add(buildFooterForIban());

        mBottomSheetFocusHelper.registerForOneTimeUse();
        mModel.set(
                SHEET_CONTENT_DESCRIPTION_ID,
                R.string.autofill_payment_method_bottom_sheet_content_description);
        mModel.set(
                SHEET_HALF_HEIGHT_DESCRIPTION_ID,
                R.string.autofill_payment_method_bottom_sheet_half_height);
        mModel.set(
                SHEET_FULL_HEIGHT_DESCRIPTION_ID,
                R.string.autofill_payment_method_bottom_sheet_full_height);
        mModel.set(
                SHEET_CLOSED_DESCRIPTION_ID, R.string.autofill_payment_method_bottom_sheet_closed);
        mModel.set(VISIBLE, true);

        RecordHistogram.recordCount100Histogram(TOUCH_TO_FILL_NUMBER_OF_IBANS_SHOWN, mIbans.size());
    }

    public void showLoyaltyCards(
            List<LoyaltyCard> affiliatedLoyaltyCards,
            List<LoyaltyCard> allLoyaltyCards,
            Function<LoyaltyCard, Drawable> valuableImageFunction,
            boolean firstTimeUsage) {
        mInputProtector.markShowTime();

        assert allLoyaltyCards != null && affiliatedLoyaltyCards != null;
        mAffiliatedLoyaltyCards = affiliatedLoyaltyCards;
        mAllLoyaltyCards = allLoyaltyCards;
        mValuableImageFunction = valuableImageFunction;
        mSuggestions = null;
        mIbans = null;
        mShouldShowScanCreditCard = false;
        mCardImageFunction = null;
        mBnplIssuerContexts = null;

        mModel.set(
                SHEET_ITEMS,
                getLoyaltyCardHomeScreenItems(
                        affiliatedLoyaltyCards, valuableImageFunction, firstTimeUsage));

        mBottomSheetFocusHelper.registerForOneTimeUse();
        mModel.set(
                SHEET_CONTENT_DESCRIPTION_ID,
                R.string.autofill_loyalty_card_bottom_sheet_content_description);
        mModel.set(
                SHEET_HALF_HEIGHT_DESCRIPTION_ID,
                R.string.autofill_loyalty_card_bottom_sheet_half_height);
        mModel.set(
                SHEET_FULL_HEIGHT_DESCRIPTION_ID,
                R.string.autofill_loyalty_card_bottom_sheet_full_height);
        mModel.set(SHEET_CLOSED_DESCRIPTION_ID, R.string.autofill_loyalty_card_bottom_sheet_closed);
        mModel.set(VISIBLE, true);

        RecordHistogram.recordCount100Histogram(
                TOUCH_TO_FILL_NUMBER_OF_AFFILIATED_LOYALTY_CARDS_SHOWN,
                mAffiliatedLoyaltyCards.size());
        RecordHistogram.recordCount100Histogram(
                TOUCH_TO_FILL_NUMBER_OF_LOYALTY_CARDS_SHOWN, mAllLoyaltyCards.size());
    }

    private ModelList getLoyaltyCardHomeScreenItems(
            List<LoyaltyCard> affiliatedLoyaltyCards,
            Function<LoyaltyCard, Drawable> valuableImageFunction,
            boolean firstTimeUsage) {
        ModelList sheetItems = new ModelList();

        for (LoyaltyCard loyaltyCard : affiliatedLoyaltyCards) {
            final PropertyModel model = createLoyaltyCardModel(loyaltyCard, valuableImageFunction);
            sheetItems.add(new ListItem(LOYALTY_CARD, model));
        }

        if (!firstTimeUsage) {
            sheetItems.add(new ListItem(ALL_LOYALTY_CARDS, createAllLoyaltyCardsItemModel()));
        }

        if (mAffiliatedLoyaltyCards.size() == 1) {
            // Use the LOYALTY_CARD model as the property model for the fill button too.
            assert sheetItems.get(0).type == LOYALTY_CARD;
            sheetItems.add(
                    new ListItem(
                            FILL_BUTTON,
                            createButtonModel(
                                    R.string.autofill_loyalty_card_autofill_button,
                                    () ->
                                            this.onSelectedLoyaltyCard(
                                                    mAffiliatedLoyaltyCards.get(0)))));
        }

        if (firstTimeUsage) {
            sheetItems.add(new ListItem(WALLET_SETTINGS_BUTTON, createWalletSettingsButtonModel()));
        }

        sheetItems.add(0, buildHeaderForLoyaltyCards(firstTimeUsage));
        sheetItems.add(buildFooterForLoyaltyCards());

        return sheetItems;
    }

    void updateBnplPaymentMethod(
            @Nullable Long extractedAmount, boolean isAmountSupportedByAnyIssuer) {
        assert mSuggestions != null;
        // `bnplSuggestion` contains the raw data for the BNPL suggestion.
        // It is decoupled from its presentation in the UI.
        AutofillSuggestion bnplSuggestion = null;
        for (int i = 0; i < mSuggestions.size(); ++i) {
            if (mSuggestions.get(i).getSuggestionType() == SuggestionType.BNPL_ENTRY) {
                bnplSuggestion = mSuggestions.get(i);
                break;
            }
        }
        // `bnplModel` holds the properties needed to render the BNPL chip on the bottom sheet.
        // It acts as a bridge between the data and the view.
        PropertyModel bnplModel = null;
        ModelList sheetItems = mModel.get(SHEET_ITEMS);
        for (int i = 0; i < sheetItems.size(); ++i) {
            if (sheetItems.get(i).type == ItemType.BNPL) {
                bnplModel = sheetItems.get(i).model;
                break;
            }
        }

        if (bnplSuggestion == null || bnplModel == null) return;

        if (isAmountSupportedByAnyIssuer) {
            bnplSuggestion.getPaymentsPayload().setExtractedAmount(extractedAmount);
            bnplModel.set(IS_ENABLED, true);
            bnplModel.set(SECONDARY_TEXT, bnplSuggestion.getSublabel());
        } else {
            bnplSuggestion.getPaymentsPayload().setExtractedAmount(null);
            bnplModel.set(IS_ENABLED, false);
            bnplModel.set(
                    SECONDARY_TEXT,
                    getString(R.string.autofill_bnpl_suggestion_label_for_unavailable_purchase));
        }
    }

    public void showProgressScreen() {
        mModel.set(CURRENT_SCREEN, PROGRESS_SCREEN);
        ModelList progressScreenModel = new ModelList();

        progressScreenModel.add(
                buildHeaderForBnplSelectionProgress(/* isBackButtonEnabled= */ false));
        progressScreenModel.add(
                new ListItem(
                        PROGRESS_ICON,
                        createProgressIconModel(
                                R.string
                                        .autofill_pending_dialog_loading_accessibility_description)));
        // TODO(crbug.com/438784993): Add footer UI to BNPL progress screen.

        mModel.set(SHEET_ITEMS, progressScreenModel);
        mModel.set(
                SHEET_CONTENT_DESCRIPTION_ID,
                R.string.autofill_bnpl_progress_sheet_content_description);
        mModel.set(
                SHEET_HALF_HEIGHT_DESCRIPTION_ID,
                R.string.autofill_bnpl_progress_sheet_half_height);
        mModel.set(
                SHEET_FULL_HEIGHT_DESCRIPTION_ID,
                R.string.autofill_bnpl_progress_sheet_full_height);
        mModel.set(SHEET_CLOSED_DESCRIPTION_ID, R.string.autofill_bnpl_progress_sheet_closed);
        mModel.set(VISIBLE, true);
    }

    /**
     * Displays the BNPL issuer selection screen.
     *
     * <p>This method shows a bottom sheet listing the provided BNPL issuers.
     *
     * @param bnplIssuerContexts A list of {@link BnplIssuerContext} to be displayed.
     * @param footerText The text to be displayed on the footer.
     */
    public void showBnplIssuers(List<BnplIssuerContext> bnplIssuerContexts, String footerText) {
        mInputProtector.markShowTime();

        assert bnplIssuerContexts != null;
        assert footerText != null;
        mBnplIssuerContexts = bnplIssuerContexts;
        mIbans = null;
        mAffiliatedLoyaltyCards = null;
        mAllLoyaltyCards = null;
        mValuableImageFunction = null;

        mModel.set(CURRENT_SCREEN, BNPL_ISSUER_SELECTION_SCREEN);
        ModelList sheetItems = new ModelList();

        sheetItems.add(buildHeaderForBnplSelectionProgress(/* isBackButtonEnabled= */ true));

        for (BnplIssuerContext issuerContext : mBnplIssuerContexts) {
            sheetItems.add(new ListItem(BNPL_ISSUER, createBnplIssuerContextModel(issuerContext)));
        }

        sheetItems.add(buildFooterForBnplSelectionProgress(footerText, /* isInProgress= */ false));

        mModel.set(
                SHEET_CONTENT_DESCRIPTION_ID,
                R.string.autofill_bnpl_issuer_bottom_sheet_content_description);
        mModel.set(
                SHEET_HALF_HEIGHT_DESCRIPTION_ID,
                R.string.autofill_bnpl_issuer_bottom_sheet_half_height);
        mModel.set(
                SHEET_FULL_HEIGHT_DESCRIPTION_ID,
                R.string.autofill_bnpl_issuer_bottom_sheet_full_height);
        mModel.set(SHEET_CLOSED_DESCRIPTION_ID, R.string.autofill_bnpl_issuer_bottom_sheet_closed);
        mModel.set(SHEET_ITEMS, sheetItems);
        mModel.set(VISIBLE, true);
    }

    public void showErrorScreen(String title, String description) {
        mModel.set(CURRENT_SCREEN, ERROR_SCREEN);
        ModelList errorScreenModel = new ModelList();

        errorScreenModel.add(buildHeaderForError(title));
        errorScreenModel.add(
                new ListItem(ERROR_DESCRIPTION, createErrorDescriptionModel(description)));
        errorScreenModel.add(
                new ListItem(
                        FILL_BUTTON,
                        createButtonModel(
                                R.string.autofill_bnpl_error_ok_button,
                                () -> this.onErrorOkPressed())));

        mModel.set(SHEET_ITEMS, errorScreenModel);
        mModel.set(
                SHEET_CONTENT_DESCRIPTION_ID,
                R.string.autofill_bnpl_error_sheet_content_description);
        mModel.set(
                SHEET_HALF_HEIGHT_DESCRIPTION_ID, R.string.autofill_bnpl_error_sheet_half_height);
        mModel.set(
                SHEET_FULL_HEIGHT_DESCRIPTION_ID, R.string.autofill_bnpl_error_sheet_full_height);
        mModel.set(SHEET_CLOSED_DESCRIPTION_ID, R.string.autofill_bnpl_error_sheet_closed);
        mModel.set(VISIBLE, true);
    }

    /**
     * Displays the BNPL issuer ToS screen.
     *
     * <p>This method shows a bottom sheet showing the BNPL issuer ToS info.
     *
     * @param BnplIssuerTosDetail A struct with text and icon to be shown.
     */
    public void showBnplIssuerTos(BnplIssuerTosDetail bnplIssuerTosDetail) {
        ModelList sheetItems = new ModelList();

        sheetItems.add(
                buildHeaderForBnplIssuerTos(
                        GlobalNightModeStateProviderHolder.getInstance().isInNightMode()
                                ? bnplIssuerTosDetail.getHeaderIconDarkDrawableId()
                                : bnplIssuerTosDetail.getHeaderIconDrawableId(),
                        bnplIssuerTosDetail.getTitle()));
        sheetItems.add(
                new ListItem(
                        BNPL_TOS_TEXT,
                        createBnplIssuerTosTextItemModel(
                                R.drawable.checklist, bnplIssuerTosDetail.getReviewText())));
        sheetItems.add(
                new ListItem(
                        BNPL_TOS_TEXT,
                        createBnplIssuerTosTextItemModel(
                                R.drawable.receipt_long, bnplIssuerTosDetail.getApproveText())));
        sheetItems.add(
                new ListItem(
                        BNPL_TOS_TEXT,
                        createBnplIssuerTosTextItemModel(
                                R.drawable.add_link, bnplIssuerTosDetail.getLinkText())));
        sheetItems.add(buildFooterForLegalMessage(bnplIssuerTosDetail.getLegalMessages()));
        sheetItems.add(
                new ListItem(
                        FILL_BUTTON,
                        createButtonModel(
                                R.string.autofill_bnpl_tos_ok_button_label,
                                this::onBnplIssuerTosAccepted)));
        sheetItems.add(
                new ListItem(
                        TEXT_BUTTON,
                        createButtonModel(
                                R.string.autofill_bnpl_tos_bottom_sheet_cancel_button_label,
                                this::onBnplIssuerTosCancelled)));
        mModel.set(
                SHEET_CONTENT_DESCRIPTION_ID,
                R.string.autofill_bnpl_issuer_tos_bottom_sheet_content_description);
        mModel.set(
                SHEET_HALF_HEIGHT_DESCRIPTION_ID,
                R.string.autofill_bnpl_issuer_tos_bottom_sheet_half_height);
        mModel.set(
                SHEET_FULL_HEIGHT_DESCRIPTION_ID,
                R.string.autofill_bnpl_issuer_tos_bottom_sheet_full_height);
        mModel.set(
                SHEET_CLOSED_DESCRIPTION_ID, R.string.autofill_bnpl_issuer_tos_bottom_sheet_closed);
        mModel.set(CURRENT_SCREEN, BNPL_ISSUER_TOS_SCREEN);
        mModel.set(SHEET_ITEMS, sheetItems);
        mModel.set(VISIBLE, true);
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
            } else if (mAffiliatedLoyaltyCards != null && mAllLoyaltyCards != null) {
                recordTouchToFillLoyaltyCardOutcomeHistogram(TouchToFillLoyaltyCardOutcome.DISMISS);
            }
            // If all possible payment methods are null, then nothing is recorded on dismissal.
        }
    }

    void showHomeScreen() {
        mModel.set(CURRENT_SCREEN, HOME_SCREEN);
        if (mSuggestions != null) {
            // TODO(crbug.com/438784993): Disable and grey out BNPL chip if no issuers are available
            // for the transaction.
            setPaymentMethodsHomeScreenItems();
        } else if (mAffiliatedLoyaltyCards != null) {
            mModel.set(FOCUSED_VIEW_ID_FOR_ACCESSIBILITY, R.id.all_loyalty_cards_item);
            mModel.set(
                    SHEET_ITEMS,
                    getLoyaltyCardHomeScreenItems(
                            mAffiliatedLoyaltyCards,
                            mValuableImageFunction,
                            /* firstTimeUsage= */ false));
        } else {
            assert false : "Unhandled home screen show";
        }
    }

    public void scanCreditCard() {
        mDelegate.scanCreditCard();
        recordTouchToFillCreditCardOutcomeHistogram(TouchToFillCreditCardOutcome.SCAN_NEW_CARD);
    }

    // TODO(crbug.com/430575808): Log `MANAGE_PAYMENTS` outcome metric for BNPL.
    public void showPaymentMethodSettings() {
        mDelegate.showPaymentMethodSettings();
        if (mSuggestions != null) {
            recordTouchToFillCreditCardOutcomeHistogram(
                    TouchToFillCreditCardOutcome.MANAGE_PAYMENTS);
        } else if (mIbans != null) {
            recordTouchToFillIbanOutcomeHistogram(TouchToFillIbanOutcome.MANAGE_PAYMENTS);
        }
    }

    public void showGoogleWalletSettings() {
        assert mAffiliatedLoyaltyCards != null && mAllLoyaltyCards != null;
        recordTouchToFillLoyaltyCardOutcomeHistogram(TouchToFillLoyaltyCardOutcome.WALLET_SETTINGS);
        mDelegate.showGoogleWalletSettings();
    }

    public void showManageLoyaltyCards() {
        assert mAffiliatedLoyaltyCards != null && mAllLoyaltyCards != null;
        mDelegate.openPassesManagementUi();
        recordTouchToFillLoyaltyCardOutcomeHistogram(
                TouchToFillLoyaltyCardOutcome.MANAGE_LOYALTY_CARDS);
    }

    private void onSelectedCreditCard(AutofillSuggestion suggestion) {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        boolean isVirtualCard =
                suggestion.getSuggestionType() == SuggestionType.VIRTUAL_CREDIT_CARD_ENTRY;
        PaymentsPayload payload = suggestion.getPaymentsPayload();
        if (payload != null) {
            mDelegate.creditCardSuggestionSelected(payload.getGuid(), isVirtualCard);
        }
        recordTouchToFillCreditCardOutcomeHistogram(
                isVirtualCard
                        ? TouchToFillCreditCardOutcome.VIRTUAL_CARD
                        : TouchToFillCreditCardOutcome.CREDIT_CARD);
        RecordHistogram.recordCount100Histogram(
                TOUCH_TO_FILL_CREDIT_CARD_INDEX_SELECTED, mSuggestions.indexOf(suggestion));
    }

    private void onSelectedIban(Iban iban) {
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

    private void onSelectedLoyaltyCard(LoyaltyCard loyaltyCard) {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        mDelegate.loyaltyCardSuggestionSelected(loyaltyCard);
        final boolean affiliatedLoyaltyCardSelected = mAffiliatedLoyaltyCards.contains(loyaltyCard);
        recordTouchToFillLoyaltyCardOutcomeHistogram(
                affiliatedLoyaltyCardSelected
                        ? TouchToFillLoyaltyCardOutcome.AFFILIATED_LOYALTY_CARD
                        : TouchToFillLoyaltyCardOutcome.NON_AFFILIATED_LOYALTY_CARD);
        if (mModel.get(CURRENT_SCREEN) == HOME_SCREEN) {
            RecordHistogram.recordCount100Histogram(
                    TOUCH_TO_FILL_AFFILIATED_LOYALTY_CARDS_SCREEN_INDEX_SELECTED,
                    mAffiliatedLoyaltyCards.indexOf(loyaltyCard));
        } else {
            RecordHistogram.recordCount100Histogram(
                    TOUCH_TO_FILL_ALL_LOYALTY_CARDS_SCREEN_INDEX_SELECTED,
                    mAllLoyaltyCards.indexOf(loyaltyCard));
        }
    }

    private void onAcceptedBnplSuggestion(AutofillSuggestion suggestion) {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        mDelegate.bnplSuggestionSelected(suggestion.getPaymentsPayload().getExtractedAmount());
    }

    private void onAcceptedBnplIssuer(String issuerId) {
        // TODO(crbug.com/430575808): During implementation, make sure that when we hide the bottom
        // sheet to prepare to show the ephemeral tab, we use `mModel.set(VISIBLE, false)` instead
        // of `hideSheet()`. This preserves the TouchToFill view and delegate on native side, which
        // is needed when the user completes the flow on the ephemeral tab and reopens the TTF
        // bottom sheet.
        if (!mInputProtector.shouldInputBeProcessed()) {
            return;
        }
        mDelegate.onBnplIssuerSuggestionSelected(issuerId);
    }

    private void onErrorOkPressed() {
        if (!mInputProtector.shouldInputBeProcessed()) {
            return;
        }
        mDelegate.onErrorOkPressed();
    }

    private void onBnplIssuerTosAccepted() {
        // TODO(crbug.com/438784697): Handle ToS accepted event.
    }

    private void onBnplIssuerTosCancelled() {
        // TODO(crbug.com/438784697): Dismiss the screen and reset the BNPL flow.
    }

    private void showAllLoyaltyCards() {
        mModel.set(CURRENT_SCREEN, ALL_LOYALTY_CARDS_SCREEN);
        mModel.set(FOCUSED_VIEW_ID_FOR_ACCESSIBILITY, R.id.all_loyalty_cards_back_image_button);
        ModelList allLoyaltyCardsModel = new ModelList();
        for (LoyaltyCard loyaltyCard : mAllLoyaltyCards) {
            final PropertyModel loyaltyCardModel =
                    createLoyaltyCardModel(loyaltyCard, mValuableImageFunction);
            allLoyaltyCardsModel.add(new ListItem(LOYALTY_CARD, loyaltyCardModel));
        }
        mModel.set(SHEET_ITEMS, allLoyaltyCardsModel);
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
        String labelDescription = "";
        PaymentsPayload payload = suggestion.getPaymentsPayload();
        if (payload != null) {
            labelDescription = payload.getLabelContentDescription();
        }
        TouchToFillPaymentMethodProperties.CardImageMetaData cardImageMetaData =
                new TouchToFillPaymentMethodProperties.CardImageMetaData(drawableId, artUrl);
        PropertyModel.Builder creditCardSuggestionModelBuilder =
                new PropertyModel.Builder(NON_TRANSFORMING_CREDIT_CARD_SUGGESTION_KEYS)
                        .withTransformingKey(CARD_IMAGE, cardImageFunction, cardImageMetaData)
                        .with(MAIN_TEXT, suggestion.getLabel())
                        .with(MAIN_TEXT_CONTENT_DESCRIPTION, labelDescription)
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

    private PropertyModel createBnplSuggestionModel(
            AutofillSuggestion suggestion, FillableItemCollectionInfo itemCollectionInfo) {
        PropertyModel.Builder bnplSuggestionModelBuilder =
                new PropertyModel.Builder(NON_TRANSFORMING_BNPL_SUGGESTION_KEYS)
                        .with(BNPL_ICON_ID, suggestion.getIconId())
                        .with(PRIMARY_TEXT, suggestion.getLabel())
                        .with(SECONDARY_TEXT, suggestion.getSublabel())
                        .with(ON_BNPL_CLICK_ACTION, () -> this.onAcceptedBnplSuggestion(suggestion))
                        .with(BNPL_ITEM_COLLECTION_INFO, itemCollectionInfo)
                        .with(IS_ENABLED, !suggestion.applyDeactivatedStyle());
        return bnplSuggestionModelBuilder.build();
    }

    private PropertyModel createBnplIssuerContextModel(BnplIssuerContext issuerContext) {
        PropertyModel.Builder bnplIssuerModelBuilder =
                new PropertyModel.Builder(NON_TRANSFORMING_BNPL_ISSUER_CONTEXT_KEYS)
                        .with(ISSUER_NAME, issuerContext.getDisplayName())
                        .with(ISSUER_SELECTION_TEXT, issuerContext.getSelectionText())
                        .with(ISSUER_ICON_ID, issuerContext.getIconId())
                        .with(ISSUER_LINKED, issuerContext.isLinked())
                        .with(
                                ON_ISSUER_CLICK_ACTION,
                                () -> this.onAcceptedBnplIssuer(issuerContext.getIssuerId()))
                        .with(APPLY_ISSUER_DEACTIVATED_STYLE, !issuerContext.isEligible());
        return bnplIssuerModelBuilder.build();
    }

    private PropertyModel createIbanModel(Iban iban) {
        PropertyModel.Builder ibanModelBuilder =
                new PropertyModel.Builder(NON_TRANSFORMING_IBAN_KEYS)
                        .with(IBAN_VALUE, iban.getLabel())
                        .with(IBAN_NICKNAME, iban.getNickname())
                        .with(ON_IBAN_CLICK_ACTION, () -> this.onSelectedIban(iban));
        return ibanModelBuilder.build();
    }

    private PropertyModel createLoyaltyCardModel(
            LoyaltyCard loyaltyCard, Function<LoyaltyCard, Drawable> valuableImageFunction) {
        PropertyModel.Builder loyaltyCardModelBuilder =
                new PropertyModel.Builder(NON_TRANSFORMING_LOYALTY_CARD_KEYS)
                        .withTransformingKey(LOYALTY_CARD_ICON, valuableImageFunction, loyaltyCard)
                        .with(LOYALTY_CARD_NUMBER, loyaltyCard.getLoyaltyCardNumber())
                        .with(MERCHANT_NAME, loyaltyCard.getMerchantName())
                        .with(
                                ON_LOYALTY_CARD_CLICK_ACTION,
                                () -> this.onSelectedLoyaltyCard(loyaltyCard));

        return loyaltyCardModelBuilder.build();
    }

    private PropertyModel createAllLoyaltyCardsItemModel() {
        return new PropertyModel.Builder(AllLoyaltyCardsItemProperties.ALL_KEYS)
                .with(AllLoyaltyCardsItemProperties.ON_CLICK_ACTION, this::showAllLoyaltyCards)
                .build();
    }

    private PropertyModel createProgressIconModel(@StringRes int contentDescriptionId) {
        return new PropertyModel.Builder(ProgressIconProperties.ALL_KEYS)
                .with(PROGRESS_CONTENT_DESCRIPTION_ID, contentDescriptionId)
                .build();
    }

    private PropertyModel createErrorDescriptionModel(String description) {
        return new PropertyModel.Builder(ErrorDescriptionProperties.ALL_KEYS)
                .with(ERROR_DESCRIPTION_STRING, description)
                .build();
    }

    private PropertyModel createBnplIssuerTosTextItemModel(int iconDrawableId, CharSequence text) {
        return new PropertyModel.Builder(BnplIssuerTosTextItemProperties.ALL_KEYS)
                .with(BnplIssuerTosTextItemProperties.BNPL_TOS_ICON_ID, iconDrawableId)
                .with(BnplIssuerTosTextItemProperties.DESCRIPTION_TEXT, text)
                .build();
    }

    private PropertyModel createButtonModel(@StringRes int titleId, Runnable onClickAction) {
        return new PropertyModel.Builder(ButtonProperties.ALL_KEYS)
                .with(TEXT_ID, titleId)
                .with(ON_CLICK_ACTION, onClickAction)
                .build();
    }

    private PropertyModel createWalletSettingsButtonModel() {
        return new PropertyModel.Builder(ButtonProperties.ALL_KEYS)
                .with(TEXT_ID, R.string.autofill_loyalty_card_wallet_settings_button)
                .with(ON_CLICK_ACTION, this::showGoogleWalletSettings)
                .build();
    }

    private ListItem buildCardBenefitTermsLabel() {
        return new ListItem(
                TERMS_LABEL,
                new PropertyModel.Builder(TermsLabelProperties.ALL_TERMS_LABEL_KEYS)
                        .with(
                                TERMS_LABEL_TEXT_ID,
                                R.string.autofill_payment_method_bottom_sheet_benefits_terms_label)
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

    private ListItem buildHeaderForLoyaltyCards(boolean firstTimeUsage) {
        @Nullable
        final TouchToFillResourceProvider resourceProvider =
                ServiceLoaderUtil.maybeCreate(TouchToFillResourceProvider.class);
        @DrawableRes
        final int headerImageId =
                resourceProvider == null
                        ? R.drawable.touch_to_fill_default_header_image
                        : resourceProvider.getLoyaltyCardHeaderDrawableId();
        PropertyModel.Builder headerBuilder =
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(IMAGE_DRAWABLE_ID, headerImageId)
                        .with(
                                TITLE_ID,
                                firstTimeUsage
                                        ? R.string
                                                .autofill_loyalty_card_first_time_usage_bottom_sheet_title
                                        : R.string.autofill_loyalty_card_bottom_sheet_title);
        if (firstTimeUsage) {
            headerBuilder.with(
                    SUBTITLE_ID,
                    R.string.autofill_loyalty_card_first_time_usage_bottom_sheet_subtitle);
        }
        return new ListItem(HEADER, headerBuilder.build());
    }

    private ListItem buildHeaderForError(String title) {
        return new ListItem(
                HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(IMAGE_DRAWABLE_ID, R.drawable.error_icon)
                        .with(TITLE_STRING, title)
                        .build());
    }

    private ListItem buildHeaderForBnplSelectionProgress(boolean isBackButtonEnabled) {
        PropertyModel.Builder bnplSelectionProgressHeaderBuilder =
                new PropertyModel.Builder(BnplSelectionProgressHeaderProperties.ALL_KEYS)
                        .with(
                                BnplSelectionProgressHeaderProperties.BNPL_BACK_BUTTON_ENABLED,
                                isBackButtonEnabled)
                        .with(
                                BnplSelectionProgressHeaderProperties.BNPL_ON_BACK_BUTTON_CLICKED,
                                () -> this.showHomeScreen());
        return new ListItem(
                BNPL_SELECTION_PROGRESS_HEADER, bnplSelectionProgressHeaderBuilder.build());
    }

    private ListItem buildHeaderForBnplIssuerTos(int issuerImageId, String title) {
        return new ListItem(
                HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(IMAGE_DRAWABLE_ID, issuerImageId)
                        .with(TITLE_STRING, title)
                        .build());
    }

    private ListItem buildFooterForCreditCard(boolean hasScanCardButton) {
        return new ListItem(
                FOOTER,
                new PropertyModel.Builder(FooterProperties.ALL_KEYS)
                        .with(SHOULD_SHOW_SCAN_CREDIT_CARD, hasScanCardButton)
                        .with(SCAN_CREDIT_CARD_CALLBACK, this::scanCreditCard)
                        .with(
                                OPEN_MANAGEMENT_UI_TITLE_ID,
                                R.string.autofill_bottom_sheet_manage_payment_methods)
                        .with(OPEN_MANAGEMENT_UI_CALLBACK, this::showPaymentMethodSettings)
                        .build());
    }

    private ListItem buildFooterForIban() {
        return new ListItem(
                FOOTER,
                new PropertyModel.Builder(FooterProperties.ALL_KEYS)
                        .with(
                                OPEN_MANAGEMENT_UI_TITLE_ID,
                                R.string.autofill_bottom_sheet_manage_payment_methods)
                        .with(OPEN_MANAGEMENT_UI_CALLBACK, this::showPaymentMethodSettings)
                        .build());
    }

    private ListItem buildFooterForLoyaltyCards() {
        return new ListItem(
                FOOTER,
                new PropertyModel.Builder(FooterProperties.ALL_KEYS)
                        .with(
                                OPEN_MANAGEMENT_UI_TITLE_ID,
                                R.string.autofill_bottom_sheet_manage_loyalty_cards)
                        .with(OPEN_MANAGEMENT_UI_CALLBACK, this::showManageLoyaltyCards)
                        .build());
    }

    private ListItem buildFooterForBnplSelectionProgress(String footerText, boolean isInProgress) {
        return new ListItem(
                BNPL_SELECTION_PROGRESS_FOOTER,
                new PropertyModel.Builder(BnplSelectionProgressFooterProperties.ALL_KEYS)
                        .with(TERMS_TEXT_ID, R.string.autofill_bnpl_issuer_bottom_sheet_terms_label)
                        .with(HIDE_OPTIONS_LINK_TEXT, footerText)
                        .with(ON_LINK_CLICK_CALLBACK, (view) -> showPaymentMethodSettings())
                        .with(APPLY_LINK_DEACTIVATED_STYLE, isInProgress)
                        .build());
    }

    private ListItem buildFooterForLegalMessage(BnplIssuerTosDetail.LegalMessages legalMessages) {
        return new ListItem(
                TOS_FOOTER,
                new PropertyModel.Builder(TosFooterProperties.ALL_KEYS)
                        .with(LEGAL_MESSAGE, legalMessages)
                        .build());
    }

    private static boolean hasOnlyLocalCards(List<AutofillSuggestion> suggestions) {
        for (AutofillSuggestion suggestion : suggestions) {
            PaymentsPayload payload = suggestion.getPaymentsPayload();
            if (payload != null && !payload.isLocalPaymentsMethod()) {
                return false;
            }
        }
        return true;
    }

    private static String getString(@StringRes int messageId) {
        return ContextUtils.getApplicationContext().getString(messageId);
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

    private static void recordTouchToFillLoyaltyCardOutcomeHistogram(
            @TouchToFillLoyaltyCardOutcome int outcome) {
        RecordHistogram.recordEnumeratedHistogram(
                TOUCH_TO_FILL_LOYALTY_CARD_OUTCOME_HISTOGRAM,
                outcome,
                TouchToFillIbanOutcome.MAX_VALUE);
    }

    void setInputProtectorForTesting(InputProtector inputProtector) {
        mInputProtector = inputProtector;
    }
}

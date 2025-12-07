// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.autofill.AutofillUiUtils.openLink;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.APPLY_ISSUER_DEACTIVATED_STYLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_ICON_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_LINKED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_SELECTION_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.NON_TRANSFORMING_BNPL_ISSUER_CONTEXT_KEYS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ON_ISSUER_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressTermsProperties.APPLY_LINK_DEACTIVATED_STYLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressTermsProperties.HIDE_OPTIONS_LINK_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressTermsProperties.ON_LINK_CLICK_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressTermsProperties.TERMS_TEXT_ID;
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
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL_SELECTION_PROGRESS_HEADER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL_SELECTION_PROGRESS_TERMS;
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
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TosFooterProperties.LEGAL_MESSAGE_LINES;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TosFooterProperties.LINK_OPENER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.VISIBLE;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.night_mode.GlobalNightModeStateProviderHolder;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
import org.chromium.chrome.browser.touch_to_fill.common.TouchToFillResourceProvider;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodComponent.Delegate;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.AllLoyaltyCardsItemProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerTosTextItemProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressHeaderProperties;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressTermsProperties;
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
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.payments.ui.InputProtector;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
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

    /** The user actions on the BNPL ToS Touch To Fill sheet. */
    @IntDef({
        TouchToFillBnplTosScreenUserAction.SHOWN,
        TouchToFillBnplTosScreenUserAction.ACCEPTED,
        TouchToFillBnplTosScreenUserAction.DISMISSED,
        TouchToFillBnplTosScreenUserAction.WALLET_LINK_CLICKED,
        TouchToFillBnplTosScreenUserAction.LEGAL_MESSAGE_LINK_CLICKED
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface TouchToFillBnplTosScreenUserAction {
        int SHOWN = 0;
        int ACCEPTED = 1;
        int DISMISSED = 2;
        int WALLET_LINK_CLICKED = 3;
        int LEGAL_MESSAGE_LINK_CLICKED = 4;
        int MAX_VALUE = LEGAL_MESSAGE_LINK_CLICKED;
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

    @VisibleForTesting
    static final String TOUCH_TO_FILL_BNPL_SELECT_ISSUER_NUMBER_OF_ISSUERS_SHOWN =
            "Autofill.TouchToFill.Bnpl.SelectIssuerScreen.NumberOfIssuersShown";

    // LINT.IfChange

    // TODO(crbug.com/438785863): Add ToS user actions.
    @VisibleForTesting
    static final String TOUCH_TO_FILL_BNPL_USER_ACTION = "Autofill.TouchToFill.Bnpl.UserAction";

    @VisibleForTesting
    static final String ISSUER_SELECTION_SCREEN_SHOWN = ".IssuerSelectionScreen.Shown";

    @VisibleForTesting
    static final String ISSUER_SELECTION_SCREEN_DISMISSED = ".IssuerSelectionScreen.Dismissed";

    @VisibleForTesting
    static final String ISSUER_SELECTION_SCREEN_AFFIRM_LINKED_SELECTED =
            ".IssuerSelectionScreen.AffirmLinkedSelected";

    @VisibleForTesting
    static final String ISSUER_SELECTION_SCREEN_AFFIRM_UNLINKED_SELECTED =
            ".IssuerSelectionScreen.AffirmUnlinkedSelected";

    @VisibleForTesting
    static final String ISSUER_SELECTION_SCREEN_KLARNA_LINKED_SELECTED =
            ".IssuerSelectionScreen.KlarnaLinkedSelected";

    @VisibleForTesting
    static final String ISSUER_SELECTION_SCREEN_KLARNA_UNLINKED_SELECTED =
            ".IssuerSelectionScreen.KlarnaUnlinkedSelected";

    @VisibleForTesting
    static final String ISSUER_SELECTION_SCREEN_ZIP_LINKED_SELECTED =
            ".IssuerSelectionScreen.ZipLinkedSelected";

    @VisibleForTesting
    static final String ISSUER_SELECTION_SCREEN_ZIP_UNLINKED_SELECTED =
            ".IssuerSelectionScreen.ZipUnlinkedSelected";

    @VisibleForTesting
    static final String ISSUER_SELECTION_SCREEN_BACK_BUTTON_SELECTED =
            ".IssuerSelectionScreen.BackButtonSelected";

    @VisibleForTesting
    static final String ISSUER_SELECTION_SCREEN_SETTINGS_LINK_SELECTED =
            ".IssuerSelectionScreen.SettingsLinkSelected";

    @VisibleForTesting static final String PROGRESS_SCREEN_SHOWN = ".ProgressScreen.Shown";

    @VisibleForTesting static final String PROGRESS_SCREEN_DISMISSED = ".ProgressScreen.Dismissed";

    @VisibleForTesting static final String ERROR_SCREEN_SHOWN = ".ErrorScreen.Shown";

    @VisibleForTesting static final String ERROR_SCREEN_DISMISSED = ".ErrorScreen.Dismissed";

    @VisibleForTesting static final String AFFIRM_TOS_SCREEN = ".AffirmTosScreen";

    @VisibleForTesting static final String KLARNA_TOS_SCREEN = ".KlarnaTosScreen";

    @VisibleForTesting static final String ZIP_TOS_SCREEN = ".ZipTosScreen";

    @VisibleForTesting static final String SCREEN_SHOWN = ".Shown";

    @VisibleForTesting static final String SCREEN_ACCEPTED = ".Accepted";

    @VisibleForTesting static final String SCREEN_DISMISSED = ".Dismissed";

    @VisibleForTesting static final String WALLET_LINK_CLICKED = ".WalletLinkClicked";

    @VisibleForTesting static final String LEGAL_MESSAGE_LINK_CLICKED = ".LegalMessageLinkClicked";

    // LINT.ThenChange(/tools/metrics/actions/actions.xml)

    // LINT.IfChange
    private static final String WALLET_LINK_TEXT = "wallet.google.com";

    private static final String WALLET_URL = "https://wallet.google.com/";
    // LINT.ThenChange(//components/autofill/core/browser/ui/payments/bnpl_tos_controller_impl.cc)

    private Context mContext;
    private TouchToFillPaymentMethodComponent.Delegate mDelegate;
    private PropertyModel mModel;
    private List<AutofillSuggestion> mSuggestions;
    private List<Iban> mIbans;
    private List<LoyaltyCard> mAffiliatedLoyaltyCards;
    private List<LoyaltyCard> mAllLoyaltyCards;
    private List<BnplIssuerContext> mBnplIssuerContexts;
    private String mBnplIssuerIdWithTosShown;
    private Function<LoyaltyCard, Drawable> mValuableImageFunction;
    private BottomSheetFocusHelper mBottomSheetFocusHelper;
    private boolean mShouldShowScanCreditCard;
    private Function<TouchToFillPaymentMethodProperties.CardImageMetaData, Drawable>
            mCardImageFunction;
    private AutofillSuggestion mBnplSuggestion;
    // It holds the properties needed to render the BNPL chip on the bottom sheet.
    // It acts as a bridge between the data and the view.
    private PropertyModel mBnplSuggestionModel;

    private InputProtector mInputProtector = new InputProtector();

    void initialize(
            Context context,
            Delegate delegate,
            PropertyModel model,
            BottomSheetFocusHelper bottomSheetFocusHelper) {
        assert context != null && delegate != null;
        mContext = context;
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
                mBnplSuggestion = suggestion;
                mBnplSuggestionModel =
                        createBnplSuggestionModel(
                                suggestion,
                                new FillableItemCollectionInfo(i + 1, mSuggestions.size()));
                sheetItems.add(new ListItem(BNPL, mBnplSuggestionModel));
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
        mModel.set(
                FOCUSED_VIEW_ID_FOR_ACCESSIBILITY, R.id.touch_to_fill_payment_method_home_screen);
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

    void onPurchaseAmountExtracted(
            List<BnplIssuerContext> bnplIssuerContexts,
            @Nullable Long extractedAmount,
            boolean isAmountSupportedByAnyIssuer) {
        assert mBnplSuggestion != null;
        if (mModel.get(CURRENT_SCREEN) == PROGRESS_SCREEN) {
            if (extractedAmount != null) {
                assert !bnplIssuerContexts.isEmpty();
                mBnplSuggestion
                        .getPaymentsPayload()
                        .setExtractedAmount(isAmountSupportedByAnyIssuer ? extractedAmount : null);
                showBnplIssuers(bnplIssuerContexts);
            } else {
                // TODO(crbug.com/438784412): If the amount exists but is not supported by any
                // issuer, we still need to gray out BNPL suggestion on the home screen.
                showErrorScreen(
                        mContext.getString(R.string.autofill_bnpl_error_dialog_title),
                        mContext.getString(R.string.autofill_bnpl_temporary_error_description));
            }
        } else {
            if (isAmountSupportedByAnyIssuer) {
                mBnplSuggestion.getPaymentsPayload().setExtractedAmount(extractedAmount);
                mBnplSuggestionModel.set(IS_ENABLED, true);
                mBnplSuggestionModel.set(SECONDARY_TEXT, mBnplSuggestion.getSublabel());
            } else {
                mBnplSuggestion.getPaymentsPayload().setExtractedAmount(null);
                mBnplSuggestionModel.set(IS_ENABLED, false);
                mBnplSuggestionModel.set(
                        SECONDARY_TEXT,
                        mContext.getString(
                                R.string.autofill_bnpl_suggestion_label_for_unavailable_purchase));
            }
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
        progressScreenModel.add(buildTermsForBnplSelectionProgress(/* isInProgress= */ true));

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
        mModel.set(FOCUSED_VIEW_ID_FOR_ACCESSIBILITY, R.id.touch_to_fill_progress_screen);
        mModel.set(VISIBLE, true);

        recordTouchToFillBnplUserAction(PROGRESS_SCREEN_SHOWN);
    }

    /**
     * Displays the BNPL issuer selection screen.
     *
     * <p>This method shows a bottom sheet listing the provided BNPL issuers.
     *
     * @param bnplIssuerContexts A list of {@link BnplIssuerContext} to be displayed.
     */
    public void showBnplIssuers(List<BnplIssuerContext> bnplIssuerContexts) {
        mInputProtector.markShowTime();

        assert bnplIssuerContexts != null;
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

        sheetItems.add(buildTermsForBnplSelectionProgress(/* isInProgress= */ false));

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
        mModel.set(
                FOCUSED_VIEW_ID_FOR_ACCESSIBILITY, R.id.touch_to_fill_bnpl_issuer_selection_screen);
        mModel.set(SHEET_ITEMS, sheetItems);
        mModel.set(VISIBLE, true);

        recordTouchToFillBnplUserAction(ISSUER_SELECTION_SCREEN_SHOWN);
        RecordHistogram.recordCount100Histogram(
                TOUCH_TO_FILL_BNPL_SELECT_ISSUER_NUMBER_OF_ISSUERS_SHOWN,
                mBnplIssuerContexts.size());
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
                                () ->
                                        onDismissed(
                                                BottomSheetController.StateChangeReason
                                                        .INTERACTION_COMPLETE))));

        mModel.set(SHEET_ITEMS, errorScreenModel);
        mModel.set(
                SHEET_CONTENT_DESCRIPTION_ID,
                R.string.autofill_bnpl_error_sheet_content_description);
        mModel.set(
                SHEET_HALF_HEIGHT_DESCRIPTION_ID, R.string.autofill_bnpl_error_sheet_half_height);
        mModel.set(
                SHEET_FULL_HEIGHT_DESCRIPTION_ID, R.string.autofill_bnpl_error_sheet_full_height);
        mModel.set(SHEET_CLOSED_DESCRIPTION_ID, R.string.autofill_bnpl_error_sheet_closed);
        mModel.set(FOCUSED_VIEW_ID_FOR_ACCESSIBILITY, R.id.touch_to_fill_error_screen);
        mModel.set(VISIBLE, true);

        recordTouchToFillBnplUserAction(ERROR_SCREEN_SHOWN);
    }

    /**
     * Displays the BNPL issuer ToS screen.
     *
     * <p>This method shows a bottom sheet showing the BNPL issuer ToS info.
     *
     * @param bnplIssuerTosDetail A struct with text and icon to be shown.
     */
    public void showBnplIssuerTos(BnplIssuerTosDetail bnplIssuerTosDetail) {
        ModelList sheetItems = new ModelList();
        String issuerName = bnplIssuerTosDetail.getIssuerName();
        mBnplIssuerIdWithTosShown = bnplIssuerTosDetail.getIssuerId();

        sheetItems.add(
                buildHeaderForBnplIssuerTos(
                        mBnplIssuerIdWithTosShown,
                        mContext.getString(
                                bnplIssuerTosDetail.getIsLinkedIssuer()
                                        ? R.string.autofill_bnpl_tos_linked_title
                                        : R.string.autofill_bnpl_tos_unlinked_title,
                                issuerName)));
        sheetItems.add(
                new ListItem(
                        BNPL_TOS_TEXT,
                        createBnplIssuerTosTextItemModel(
                                R.drawable.checklist,
                                mContext.getString(
                                        R.string.autofill_bnpl_tos_review_text, issuerName))));
        sheetItems.add(
                new ListItem(
                        BNPL_TOS_TEXT,
                        createBnplIssuerTosTextItemModel(
                                R.drawable.receipt_long,
                                mContext.getString(
                                        R.string.autofill_bnpl_tos_approve_text, issuerName))));
        sheetItems.add(
                new ListItem(
                        BNPL_TOS_TEXT,
                        createBnplIssuerTosTextItemModel(
                                R.drawable.add_link, getLinkTextForBnplTosScreen(issuerName))));
        sheetItems.add(buildFooterForLegalMessage(bnplIssuerTosDetail.getLegalMessageLines()));
        sheetItems.add(
                new ListItem(
                        FILL_BUTTON,
                        createButtonModel(
                                R.string.autofill_bnpl_tos_ok_button_label,
                                this::onBnplTosAccepted)));
        sheetItems.add(
                new ListItem(
                        TEXT_BUTTON,
                        createButtonModel(
                                R.string.autofill_bnpl_tos_bottom_sheet_cancel_button_label,
                                () ->
                                        onDismissed(
                                                BottomSheetController.StateChangeReason
                                                        .INTERACTION_COMPLETE))));
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
        mModel.set(FOCUSED_VIEW_ID_FOR_ACCESSIBILITY, R.id.touch_to_fill_bnpl_issuer_tos_screen);
        mModel.set(CURRENT_SCREEN, BNPL_ISSUER_TOS_SCREEN);
        mModel.set(SHEET_ITEMS, sheetItems);
        mModel.set(VISIBLE, true);

        recordTouchToFillBnplTosUserAction(TouchToFillBnplTosScreenUserAction.SHOWN);
    }

    void hideSheet() {
        onDismissed(BottomSheetController.StateChangeReason.NONE);
    }

    void setVisible(boolean visible) {
        mModel.set(VISIBLE, visible);
    }

    // TODO(crbug.com/461545861): Split logic by screen (e.g. BNPL_ISSUER_SELECTION_SCREEN) instead
    // of the type of payment method set (e.g. mIbans).
    public void onDismissed(@StateChangeReason int reason) {
        // TODO(b/332193789): Add IBAN-related metrics.
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        boolean dismissedByUser =
                reason == StateChangeReason.SWIPE
                        || reason == StateChangeReason.BACK_PRESS
                        || reason == StateChangeReason.TAP_SCRIM
                        || reason == StateChangeReason.INTERACTION_COMPLETE;
        // TODO(crbug.com/463785747): For now, a boolean is passed from the Java side to decide if
        // we allow showing the bottom sheet again. The ideal approach is to create a list of types
        // that can be shown again.
        mDelegate.onDismissed(dismissedByUser, shouldReshow(dismissedByUser));
        if (dismissedByUser) {
            if (mSuggestions != null) {
                if (mModel.get(CURRENT_SCREEN) == BNPL_ISSUER_SELECTION_SCREEN) {
                    recordTouchToFillBnplUserAction(ISSUER_SELECTION_SCREEN_DISMISSED);
                } else if (mModel.get(CURRENT_SCREEN) == PROGRESS_SCREEN) {
                    recordTouchToFillBnplUserAction(PROGRESS_SCREEN_DISMISSED);
                } else if (mModel.get(CURRENT_SCREEN) == ERROR_SCREEN) {
                    recordTouchToFillBnplUserAction(ERROR_SCREEN_DISMISSED);
                } else if (mModel.get(CURRENT_SCREEN) == BNPL_ISSUER_TOS_SCREEN) {
                    recordTouchToFillBnplTosUserAction(
                            TouchToFillBnplTosScreenUserAction.DISMISSED);
                }
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

    /**
     * Displays the home screen when the back button is pressed.
     *
     * <p>This method is called when the user presses the back button in the header of the bottom
     * sheet. Based on the BNPL issuers' eligibility status from the selection screen, it shows the
     * BNPL chip on the home screen as either enabled or disabled.
     */
    public void onBackButtonPressed() {
        if (mModel.get(CURRENT_SCREEN) != BNPL_ISSUER_SELECTION_SCREEN) {
            showHomeScreen();
            return;
        }
        recordTouchToFillBnplUserAction(ISSUER_SELECTION_SCREEN_BACK_BUTTON_SELECTED);
        boolean isBnplChipEnabled = false;
        ModelList sheetItems = mModel.get(SHEET_ITEMS);
        for (int i = 0; i < sheetItems.size(); ++i) {
            // If any issuer is enabled, the home screen BNPL chip remains active. Otherwise,
            // the chip is grayed out.
            if (sheetItems.get(i).type == ItemType.BNPL_ISSUER
                    && !sheetItems.get(i).model.get(APPLY_ISSUER_DEACTIVATED_STYLE)) {
                isBnplChipEnabled = true;
                break;
            }
        }
        showHomeScreen();
        assert mBnplSuggestionModel != null;
        mBnplSuggestionModel.set(IS_ENABLED, isBnplChipEnabled);
        if (!isBnplChipEnabled) {
            mBnplSuggestionModel.set(
                    SECONDARY_TEXT,
                    mContext.getString(
                            R.string.autofill_bnpl_suggestion_label_for_unavailable_purchase));
        }
    }

    public void scanCreditCard() {
        mDelegate.scanCreditCard();
        recordTouchToFillCreditCardOutcomeHistogram(TouchToFillCreditCardOutcome.SCAN_NEW_CARD);
    }

    public void showPaymentMethodSettings() {
        mDelegate.showPaymentMethodSettings();
        if (mSuggestions != null) {
            if (mModel.get(CURRENT_SCREEN) == BNPL_ISSUER_SELECTION_SCREEN) {
                recordTouchToFillBnplUserAction(ISSUER_SELECTION_SCREEN_SETTINGS_LINK_SELECTED);
            } else {
                recordTouchToFillCreditCardOutcomeHistogram(
                        TouchToFillCreditCardOutcome.MANAGE_PAYMENTS);
            }
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

    /**
     * Returns the link text for the BNPL ToS screen.
     *
     * @param issuerName The display name for the selected issuer.
     * @return The link text for the BNPL ToS screen.
     */
    protected SpannableString getLinkTextForBnplTosScreen(String issuerName) {
        return SpanApplier.applySpans(
                mContext.getString(
                        R.string.autofill_bnpl_tos_link_text, issuerName, WALLET_LINK_TEXT),
                new SpanApplier.SpanInfo(
                        "<link>",
                        "</link>",
                        new ChromeClickableSpan(
                                mContext,
                                view -> {
                                    recordTouchToFillBnplTosUserAction(
                                            TouchToFillBnplTosScreenUserAction.WALLET_LINK_CLICKED);
                                    openLink(mContext, WALLET_URL);
                                })));
    }

    // TODO(crbug.com/459842727): Split HOME_SCREEN into CREDIT_CARD_HOME_SCREEN,
    // LOYALTY_CARD_HOME_SCREEN and IBAN_HOME_SCREEN.
    private void showHomeScreen() {
        mModel.set(CURRENT_SCREEN, HOME_SCREEN);
        if (mSuggestions != null) {
            // TODO(crbug.com/438784993): Disable and grey out BNPL chip if no issuers are available
            // for the transaction.
            // TODO(crbug.com/430575808): Reset mBnplIssuerContexts when navigating back to the
            // payment method home screen after pressing the back button.
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

    private void onAcceptedBnplIssuer(String issuerId, boolean isLinked) {
        if (!mInputProtector.shouldInputBeProcessed()) {
            return;
        }
        showProgressScreen();
        mDelegate.onBnplIssuerSuggestionSelected(issuerId);

        recordTouchToFillBnplIssuerUserAction(issuerId, isLinked);
    }

    private void onBnplTosAccepted() {
        if (!mInputProtector.shouldInputBeProcessed()) {
            return;
        }
        showProgressScreen();
        mDelegate.onBnplTosAccepted();
        recordTouchToFillBnplTosUserAction(TouchToFillBnplTosScreenUserAction.ACCEPTED);
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
                                () ->
                                        this.onAcceptedBnplIssuer(
                                                issuerContext.getIssuerId(),
                                                issuerContext.isLinked()))
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
                                this::onBackButtonPressed);
        return new ListItem(
                BNPL_SELECTION_PROGRESS_HEADER, bnplSelectionProgressHeaderBuilder.build());
    }

    private ListItem buildHeaderForBnplIssuerTos(String issuerId, String title) {
        @Nullable
        final TouchToFillResourceProvider resourceProvider =
                ServiceLoaderUtil.maybeCreate(TouchToFillResourceProvider.class);
        @DrawableRes
        final int issuerImageId =
                resourceProvider == null
                        ? R.drawable.bnpl_icon_generic
                        : resourceProvider.getBnplIssuerTosDrawableId(
                                issuerId,
                                /* isLightMode= */ !GlobalNightModeStateProviderHolder.getInstance()
                                        .isInNightMode());
        return new ListItem(
                HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(
                                IMAGE_DRAWABLE_ID,
                                issuerImageId == 0 ? R.drawable.bnpl_icon_generic : issuerImageId)
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

    private ListItem buildTermsForBnplSelectionProgress(boolean isInProgress) {
        return new ListItem(
                BNPL_SELECTION_PROGRESS_TERMS,
                new PropertyModel.Builder(BnplSelectionProgressTermsProperties.ALL_KEYS)
                        .with(TERMS_TEXT_ID, R.string.autofill_bnpl_issuer_bottom_sheet_terms_label)
                        .with(
                                HIDE_OPTIONS_LINK_TEXT,
                                mContext.getString(
                                        R.string
                                                .autofill_card_bnpl_select_provider_bottom_sheet_footnote_hide_option))
                        .with(ON_LINK_CLICK_CALLBACK, (view) -> showPaymentMethodSettings())
                        .with(APPLY_LINK_DEACTIVATED_STYLE, isInProgress)
                        .build());
    }

    private ListItem buildFooterForLegalMessage(List<LegalMessageLine> legalMessageLines) {
        return new ListItem(
                TOS_FOOTER,
                new PropertyModel.Builder(TosFooterProperties.ALL_KEYS)
                        .with(LEGAL_MESSAGE_LINES, legalMessageLines)
                        .with(
                                LINK_OPENER,
                                url -> {
                                    recordTouchToFillBnplTosUserAction(
                                            TouchToFillBnplTosScreenUserAction
                                                    .LEGAL_MESSAGE_LINK_CLICKED);
                                    openLink(mContext, url);
                                })
                        .build());
    }

    private boolean shouldReshow(boolean dismissedByUser) {
        // Ensure the bottom sheet can be reshown if the user dismissed a BNPL flow.
        int currentScreen = mModel.get(CURRENT_SCREEN);
        return dismissedByUser
                && mBnplIssuerContexts != null
                && (currentScreen == BNPL_ISSUER_SELECTION_SCREEN
                        || currentScreen == BNPL_ISSUER_TOS_SCREEN
                        || currentScreen == PROGRESS_SCREEN
                        || currentScreen == ERROR_SCREEN);
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

    private static void recordTouchToFillBnplIssuerUserAction(String issuerId, boolean isLinked) {
        switch (issuerId) {
            case "affirm":
                recordTouchToFillBnplUserAction(
                        isLinked
                                ? ISSUER_SELECTION_SCREEN_AFFIRM_LINKED_SELECTED
                                : ISSUER_SELECTION_SCREEN_AFFIRM_UNLINKED_SELECTED);
                break;
            case "klarna":
                recordTouchToFillBnplUserAction(
                        isLinked
                                ? ISSUER_SELECTION_SCREEN_KLARNA_LINKED_SELECTED
                                : ISSUER_SELECTION_SCREEN_KLARNA_UNLINKED_SELECTED);
                break;
            case "zip":
                recordTouchToFillBnplUserAction(
                        isLinked
                                ? ISSUER_SELECTION_SCREEN_ZIP_LINKED_SELECTED
                                : ISSUER_SELECTION_SCREEN_ZIP_UNLINKED_SELECTED);
                break;
            default:
                // Nothing is recorded for all other issuerId's.
                break;
        }
    }

    private void recordTouchToFillBnplTosUserAction(
            @TouchToFillBnplTosScreenUserAction int userAction) {
        String tosUserAction;
        switch (mBnplIssuerIdWithTosShown) {
            case "affirm":
                tosUserAction = AFFIRM_TOS_SCREEN;
                break;
            case "klarna":
                tosUserAction = KLARNA_TOS_SCREEN;
                break;
            case "zip":
                tosUserAction = ZIP_TOS_SCREEN;
                break;
            default:
                // Nothing is recorded for all other issuerId's.
                return;
        }

        switch (userAction) {
            case TouchToFillBnplTosScreenUserAction.SHOWN:
                tosUserAction += SCREEN_SHOWN;
                break;
            case TouchToFillBnplTosScreenUserAction.ACCEPTED:
                tosUserAction += SCREEN_ACCEPTED;
                break;
            case TouchToFillBnplTosScreenUserAction.DISMISSED:
                tosUserAction += SCREEN_DISMISSED;
                break;
            case TouchToFillBnplTosScreenUserAction.WALLET_LINK_CLICKED:
                tosUserAction += WALLET_LINK_CLICKED;
                break;
            case TouchToFillBnplTosScreenUserAction.LEGAL_MESSAGE_LINK_CLICKED:
                tosUserAction += LEGAL_MESSAGE_LINK_CLICKED;
                break;
            default:
                assert false : "Undefined BNPL ToS screen user action: " + userAction;
                return;
        }

        recordTouchToFillBnplUserAction(tosUserAction);
    }

    private static void recordTouchToFillBnplUserAction(String userAction) {
        RecordUserAction.record(TOUCH_TO_FILL_BNPL_USER_ACTION + userAction);
    }

    void setInputProtectorForTesting(InputProtector inputProtector) {
        mInputProtector = inputProtector;
    }

    PropertyModel getBnplSuggestionModelForTesting() {
        return mBnplSuggestionModel;
    }
}

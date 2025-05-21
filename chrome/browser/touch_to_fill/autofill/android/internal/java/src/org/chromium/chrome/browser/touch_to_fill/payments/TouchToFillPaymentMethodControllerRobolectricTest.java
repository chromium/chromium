// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createCreditCard;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createCreditCardSuggestion;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createVirtualCreditCard;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_CREDIT_CARD_INDEX_SELECTED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_IBAN_INDEX_SELECTED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_IBAN_OUTCOME_HISTOGRAM;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_NUMBER_OF_IBANS_SHOWN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.APPLY_DEACTIVATED_STYLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.FIRST_LINE_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MAIN_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MAIN_TEXT_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MINOR_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.ON_CREDIT_CARD_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.SECOND_LINE_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SCAN_CREDIT_CARD_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.TITLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_NICKNAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_VALUE;
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
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.ON_LOYALTY_CARD_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TermsLabelProperties.CARD_BENEFITS_TERMS_AVAILABLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.VISIBLE;

import android.app.Activity;
import android.content.Context;
import android.text.TextUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.ContextUtils;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillImageFetcher;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.common.TouchToFillResourceProvider;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TouchToFillCreditCardOutcome;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TouchToFillIbanOutcome;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.LoyaltyCard;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.payments.ui.InputProtector;
import org.chromium.components.payments.ui.test_support.FakeClock;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.TimeoutException;
import java.util.stream.Collectors;
import java.util.stream.StreamSupport;

/**
 * Tests for {@link TouchToFillPaymentMethodCoordinator} and {@link
 * TouchToFillPaymentMethodMediator}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
@DisableFeatures({AutofillFeatures.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
public class TouchToFillPaymentMethodControllerRobolectricTest {
    private static final CreditCard VISA =
            createCreditCard(
                    "Visa",
                    "4111111111111111",
                    "5",
                    "2050",
                    true,
                    "Visa",
                    "• • • • 1111",
                    0,
                    "visa");
    private static final CreditCard NICKNAMED_VISA =
            createCreditCard(
                    "Visa",
                    "4111111111111111",
                    "5",
                    "2050",
                    true,
                    "Best Card",
                    "• • • • 1111",
                    0,
                    "visa");
    private static final CreditCard MASTERCARD =
            createCreditCard(
                    "MasterCard",
                    "5555555555554444",
                    "8",
                    "2050",
                    true,
                    "MasterCard",
                    "• • • • 4444",
                    0,
                    "mastercard");
    private static final CreditCard VIRTUAL_CARD =
            createVirtualCreditCard(
                    /* name= */ "Visa",
                    /* number= */ "4111111111111111",
                    /* month= */ "5",
                    /* year= */ "2050",
                    /* network= */ "Visa",
                    /* iconId= */ 0,
                    /* cardNameForAutofillDisplay= */ "Visa",
                    /* obfuscatedLastFourDigits= */ "• • • • 1111");

    private static final Iban LOCAL_IBAN =
            Iban.createLocal(
                    /* guid= */ "000000111111",
                    /* label= */ "CH56 **** **** **** *800 9",
                    /* nickname= */ "My brother's IBAN",
                    /* value= */ "CH5604835012345678009");

    private static final Iban LOCAL_IBAN_NO_NICKNAME =
            Iban.createLocal(
                    /* guid= */ "000000222222",
                    /* label= */ "FR76 **** **** **** **** ***0 189",
                    /* nickname= */ "",
                    /* value= */ "FR7630006000011234567890189");

    private static final LoyaltyCard LOYALTY_CARD_1 =
            new LoyaltyCard(
                    /* loyaltyCardId= */ "cvs",
                    /* merchantName= */ "CVS Pharmacy",
                    /* programName= */ "Loyalty program",
                    /* programLogo= */ new GURL("https://site.com/icon.png"),
                    /* loyaltyCardNumber= */ "1234",
                    /* merchantDomains= */ Collections.emptyList());

    private static final LoyaltyCard LOYALTY_CARD_2 =
            new LoyaltyCard(
                    /* loyaltyCardId= */ "stb",
                    /* merchantName= */ "Starbucks",
                    /* programName= */ "Coffee pro",
                    /* programLogo= */ new GURL("https://coffee.com/logo.png"),
                    /* loyaltyCardNumber= */ "4321",
                    /* merchantDomains= */ Collections.emptyList());

    private static final AutofillSuggestion VISA_SUGGESTION =
            createCreditCardSuggestion(
                    VISA.getCardNameForAutofillDisplay(),
                    VISA.getObfuscatedLastFourDigits(),
                    VISA.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL(""),
                    VISA.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    VISA.getGUID(),
                    VISA.getIsLocal());
    private static final AutofillSuggestion VISA_SUGGESTION_WITH_CARD_BENEFITS =
            createCreditCardSuggestion(
                    VISA.getCardNameForAutofillDisplay(),
                    VISA.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "2% cashback on travel",
                    VISA.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    VISA.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ true,
                    VISA.getGUID(),
                    VISA.getIsLocal());
    private static final AutofillSuggestion NICKNAMED_VISA_SUGGESTION =
            createCreditCardSuggestion(
                    NICKNAMED_VISA.getCardNameForAutofillDisplay(),
                    NICKNAMED_VISA.getObfuscatedLastFourDigits(),
                    NICKNAMED_VISA.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    String.format(
                            "%s %s",
                            NICKNAMED_VISA.getCardNameForAutofillDisplay(),
                            NICKNAMED_VISA.getBasicCardIssuerNetwork()),
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    NICKNAMED_VISA.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    NICKNAMED_VISA.getGUID(),
                    NICKNAMED_VISA.getIsLocal());
    private static final AutofillSuggestion MASTERCARD_SUGGESTION =
            createCreditCardSuggestion(
                    MASTERCARD.getCardNameForAutofillDisplay(),
                    MASTERCARD.getObfuscatedLastFourDigits(),
                    MASTERCARD.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    MASTERCARD.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    MASTERCARD.getGUID(),
                    MASTERCARD.getIsLocal());
    private static final AutofillSuggestion NON_ACCEPTABLE_VIRTUAL_CARD_SUGGESTION =
            createCreditCardSuggestion(
                    VIRTUAL_CARD.getCardNameForAutofillDisplay(),
                    VIRTUAL_CARD.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "Merchant doesn't accept this virtual card",
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.VIRTUAL_CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    VIRTUAL_CARD.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ true,
                    /* shouldDisplayTermsAvailable= */ false,
                    VIRTUAL_CARD.getGUID(),
                    VIRTUAL_CARD.getIsLocal());
    private static final AutofillSuggestion ACCEPTABLE_VIRTUAL_CARD_SUGGESTION =
            createCreditCardSuggestion(
                    VIRTUAL_CARD.getCardNameForAutofillDisplay(),
                    VIRTUAL_CARD.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "Virtual Card",
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.VIRTUAL_CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL(AutofillUiUtils.CAPITAL_ONE_ICON_URL),
                    VIRTUAL_CARD.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    VIRTUAL_CARD.getGUID(),
                    VIRTUAL_CARD.getIsLocal());
    private static final AutofillSuggestion VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS =
            createCreditCardSuggestion(
                    VIRTUAL_CARD.getCardNameForAutofillDisplay(),
                    VIRTUAL_CARD.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "2% cashback on travel",
                    /* secondarySubLabel= */ "Virtual card",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.VIRTUAL_CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    VIRTUAL_CARD.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ true,
                    VIRTUAL_CARD.getGUID(),
                    VIRTUAL_CARD.getIsLocal());

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final FakeClock mClock = new FakeClock();
    private TouchToFillPaymentMethodCoordinator mCoordinator;
    private PropertyModel mTouchToFillPaymentMethodModel;
    Context mContext;

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TouchToFillPaymentMethodComponent.Delegate mDelegateMock;
    @Mock private BottomSheetFocusHelper mBottomSheetFocusHelper;
    @Mock private AutofillImageFetcher mImageFetcher;
    @Mock private TouchToFillResourceProvider mResourceProvider;

    public TouchToFillPaymentMethodControllerRobolectricTest() {
        mContext = Robolectric.buildActivity(Activity.class).get();
    }

    @Before
    public void setUp() {
        ServiceLoaderUtil.setInstanceForTesting(
                TouchToFillResourceProvider.class, mResourceProvider);
        when(mResourceProvider.getLoyaltyCardHeaderDrawableId())
                .thenReturn(R.drawable.ic_globe_24dp);
        when(mBottomSheetController.requestShowContent(any(BottomSheetContent.class), anyBoolean()))
                .thenReturn(true);
        mCoordinator = new TouchToFillPaymentMethodCoordinator();
        mCoordinator.initialize(
                mContext,
                mImageFetcher,
                mBottomSheetController,
                mDelegateMock,
                mBottomSheetFocusHelper);
        mTouchToFillPaymentMethodModel = mCoordinator.getModelForTesting();
        mCoordinator
                .getMediatorForTesting()
                .setInputProtectorForTesting(new InputProtector(mClock));
    }

    @Test
    public void testAddsTheBottomSheetHelperToObserveTheSheetForCreditCard() {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);

        verify(mBottomSheetFocusHelper, times(1)).registerForOneTimeUse();
    }

    @Test
    public void testCreatesValidDefaultCreditCardModel() {
        assertNotNull(mTouchToFillPaymentMethodModel.get(SHEET_ITEMS));
        assertNotNull(mTouchToFillPaymentMethodModel.get(DISMISS_HANDLER));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(false));
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);

        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));
    }

    @Test
    public void testShowCreditCardSuggestionsWithOneEntry() throws TimeoutException {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, 1));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(1));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.fre_product_logo));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_payment_method_bottom_sheet_title));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, VISA_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertThat(cardSuggestionModel.get().get(MAIN_TEXT), is(VISA_SUGGESTION.getLabel()));
        assertThat(
                cardSuggestionModel.get().get(MINOR_TEXT), is(VISA_SUGGESTION.getSecondaryLabel()));
        assertThat(
                cardSuggestionModel.get().get(FIRST_LINE_LABEL), is(VISA_SUGGESTION.getSublabel()));
        assertFalse(cardSuggestionModel.get().get(APPLY_DEACTIVATED_STYLE));
    }

    @Test
    public void testShowCreditCardSuggestionsWithTwoEntries() throws TimeoutException {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ false);

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, 2));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(2));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.fre_product_logo));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_payment_method_bottom_sheet_title));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, VISA_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertThat(cardSuggestionModel.get().get(MAIN_TEXT), is(VISA_SUGGESTION.getLabel()));
        assertThat(
                cardSuggestionModel.get().get(MINOR_TEXT), is(VISA_SUGGESTION.getSecondaryLabel()));
        assertThat(
                cardSuggestionModel.get().get(FIRST_LINE_LABEL), is(VISA_SUGGESTION.getSublabel()));

        cardSuggestionModel = getCardSuggestionModel(itemList, MASTERCARD_SUGGESTION);
        assertThat(cardSuggestionModel.get().get(MAIN_TEXT), is(MASTERCARD_SUGGESTION.getLabel()));
        assertThat(
                cardSuggestionModel.get().get(MINOR_TEXT),
                is(MASTERCARD_SUGGESTION.getSecondaryLabel()));
        assertThat(
                cardSuggestionModel.get().get(FIRST_LINE_LABEL),
                is(MASTERCARD_SUGGESTION.getSublabel()));
    }

    @Test
    public void testShowCreditCardSuggestionsWithNonAcceptableEntries() throws TimeoutException {
        HistogramWatcher metricsWatcher =
                HistogramWatcher.newSingleRecordWatcher(TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, 2);
        mCoordinator.showCreditCards(
                List.of(NON_ACCEPTABLE_VIRTUAL_CARD_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ false);

        metricsWatcher.assertExpected();

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(2));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.google_pay));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_payment_method_bottom_sheet_title));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, NON_ACCEPTABLE_VIRTUAL_CARD_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertTrue(cardSuggestionModel.get().get(APPLY_DEACTIVATED_STYLE));

        cardSuggestionModel = getCardSuggestionModel(itemList, MASTERCARD_SUGGESTION);
        assertFalse(cardSuggestionModel.get().get(APPLY_DEACTIVATED_STYLE));
    }

    @Test
    public void testShowCreditCardSuggestionsWithCardBenefits() throws TimeoutException {
        mCoordinator.showCreditCards(
                List.of(
                        MASTERCARD_SUGGESTION,
                        VISA_SUGGESTION_WITH_CARD_BENEFITS,
                        VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS),
                /* shouldShowScanCreditCard= */ false);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(3));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.google_pay));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_payment_method_bottom_sheet_title));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, MASTERCARD_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertThat(cardSuggestionModel.get().get(MAIN_TEXT), is(MASTERCARD_SUGGESTION.getLabel()));
        assertThat(
                cardSuggestionModel.get().get(MINOR_TEXT),
                is(MASTERCARD_SUGGESTION.getSecondaryLabel()));
        assertThat(
                cardSuggestionModel.get().get(FIRST_LINE_LABEL),
                is(MASTERCARD_SUGGESTION.getSublabel()));
        // If card benefits are not present, the second line in labels has empty value.
        assertTrue(cardSuggestionModel.get().get(SECOND_LINE_LABEL).isEmpty());

        cardSuggestionModel = getCardSuggestionModel(itemList, VISA_SUGGESTION_WITH_CARD_BENEFITS);
        assertTrue(cardSuggestionModel.isPresent());
        assertThat(
                cardSuggestionModel.get().get(MAIN_TEXT),
                is(VISA_SUGGESTION_WITH_CARD_BENEFITS.getLabel()));
        assertThat(
                cardSuggestionModel.get().get(MINOR_TEXT),
                is(VISA_SUGGESTION_WITH_CARD_BENEFITS.getSecondaryLabel()));
        assertThat(
                cardSuggestionModel.get().get(FIRST_LINE_LABEL),
                is(VISA_SUGGESTION_WITH_CARD_BENEFITS.getSublabel()));
        assertThat(
                cardSuggestionModel.get().get(SECOND_LINE_LABEL),
                is(VISA_SUGGESTION_WITH_CARD_BENEFITS.getSecondarySublabel()));

        cardSuggestionModel =
                getCardSuggestionModel(itemList, VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS);
        assertThat(
                cardSuggestionModel.get().get(MAIN_TEXT),
                is(VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS.getLabel()));
        assertThat(
                cardSuggestionModel.get().get(MINOR_TEXT),
                is(VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS.getSecondaryLabel()));
        assertThat(
                cardSuggestionModel.get().get(FIRST_LINE_LABEL),
                is(VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS.getSublabel()));
        assertThat(
                cardSuggestionModel.get().get(SECOND_LINE_LABEL),
                is(VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS.getSecondarySublabel()));
    }

    @Test
    public void testBenefitsTermsLabel_ShownWhenCardHasBenefits() {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION_WITH_CARD_BENEFITS), /* shouldShowScanCreditCard= */ true);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        List<PropertyModel> termsLabelModelList = getModelsOfType(itemList, TERMS_LABEL);
        assertEquals(1, termsLabelModelList.size());
        assertTrue(termsLabelModelList.get(0).get(CARD_BENEFITS_TERMS_AVAILABLE));
    }

    @Test
    public void testBenefitsTermsLabel_HiddenWhenCardHasNoBenefits() {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ true);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(0, getModelsOfType(itemList, TERMS_LABEL).size());
    }

    @Test
    public void testScanNewCardIsShownForCreditCards() {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);
        int lastItemPos = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS).size() - 1;
        mTouchToFillPaymentMethodModel
                .get(SHEET_ITEMS)
                .get(lastItemPos)
                .model
                .get(SCAN_CREDIT_CARD_CALLBACK)
                .run();
        verify(mDelegateMock).scanCreditCard();
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.SCAN_NEW_CARD));
    }

    @Test
    public void testShowPaymentMethodSettingsForCreditCards() {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);
        int lastItemPos = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS).size() - 1;
        mTouchToFillPaymentMethodModel
                .get(SHEET_ITEMS)
                .get(lastItemPos)
                .model
                .get(SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK)
                .run();
        verify(mDelegateMock).showPaymentMethodSettings();
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.MANAGE_PAYMENTS));
    }

    @Test
    public void testNoCallbackForCreditCardSuggestionOnSelectingItemBeforeInputTime() {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(
                        mTouchToFillPaymentMethodModel.get(SHEET_ITEMS), VISA_SUGGESTION);
        assertNotNull(cardSuggestionModel.get().get(ON_CREDIT_CARD_CLICK_ACTION));

        // Clicking after an interval less than the threshold should be a no-op.
        mClock.advanceCurrentTimeMillis(
                InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD - 100);
        cardSuggestionModel.get().get(ON_CREDIT_CARD_CLICK_ACTION).run();
        verify(mDelegateMock, times(0))
                .creditCardSuggestionSelected(VISA.getGUID(), VISA.getIsVirtual());

        // Clicking after the threshold should work.
        mClock.advanceCurrentTimeMillis(100);
        cardSuggestionModel.get().get(ON_CREDIT_CARD_CLICK_ACTION).run();
        verify(mDelegateMock, times(1))
                .creditCardSuggestionSelected(VISA.getGUID(), VISA.getIsVirtual());
    }

    @Test
    public void testCallsCallbackForCreditCardSuggestionOnSelectingItem() {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(
                        mTouchToFillPaymentMethodModel.get(SHEET_ITEMS), VISA_SUGGESTION);
        assertNotNull(cardSuggestionModel.get().get(ON_CREDIT_CARD_CLICK_ACTION));

        advanceClockAndClick(cardSuggestionModel.get());
        verify(mDelegateMock).creditCardSuggestionSelected(VISA.getGUID(), VISA.getIsVirtual());
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.CREDIT_CARD));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_CREDIT_CARD_INDEX_SELECTED, 0));
    }

    @Test
    public void testCallsCallbackForVirtualCardSuggestionOnSelectingItem() {
        mCoordinator.showCreditCards(
                List.of(ACCEPTABLE_VIRTUAL_CARD_SUGGESTION), /* shouldShowScanCreditCard= */ false);
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(
                        mTouchToFillPaymentMethodModel.get(SHEET_ITEMS),
                        ACCEPTABLE_VIRTUAL_CARD_SUGGESTION);
        assertNotNull(cardSuggestionModel.get().get(ON_CREDIT_CARD_CLICK_ACTION));

        advanceClockAndClick(cardSuggestionModel.get());
        verify(mDelegateMock)
                .creditCardSuggestionSelected(VIRTUAL_CARD.getGUID(), VIRTUAL_CARD.getIsVirtual());
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.VIRTUAL_CARD));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_CREDIT_CARD_INDEX_SELECTED, 0));
    }

    @Test
    public void testShowsContinueButtonWhenOneCreditCard() {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ true);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(1, getModelsOfType(itemList, FILL_BUTTON).size());
    }

    @Test
    public void testNoContinueButtonWhenManyCreditCards() {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(0, getModelsOfType(itemList, FILL_BUTTON).size());
    }

    @Test
    public void testDismissWithSwipeForCreditCard() {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);

        mTouchToFillPaymentMethodModel.get(DISMISS_HANDLER).onResult(StateChangeReason.SWIPE);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.DISMISS));
    }

    @Test
    public void testDismissWithTapForCreditCard() {
        HistogramWatcher metricsWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.DISMISS);
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);

        mTouchToFillPaymentMethodModel.get(DISMISS_HANDLER).onResult(StateChangeReason.TAP_SCRIM);

        metricsWatcher.assertExpected();
    }

    @Test
    public void testScanNewCardClick() {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        getModelsOfType(itemList, FOOTER).get(0).get(SCAN_CREDIT_CARD_CALLBACK).run();

        verify(mDelegateMock).scanCreditCard();
    }

    @Test
    public void testManagePaymentMethodsClickForCreditCard() {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ false);
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        getModelsOfType(itemList, FOOTER).get(0).get(SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK).run();

        verify(mDelegateMock).showPaymentMethodSettings();
    }

    @Test
    public void testContinueButtonClickForCreditCard() {
        mCoordinator.showCreditCards(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        advanceClockAndClick(getModelsOfType(itemList, FILL_BUTTON).get(0));
        verify(mDelegateMock).creditCardSuggestionSelected(VISA.getGUID(), VISA.getIsVirtual());
    }

    @Test
    public void testCardSuggestionModelForNicknamedCardContainsANetworkName() {
        mCoordinator.showCreditCards(
                List.of(NICKNAMED_VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, NICKNAMED_VISA_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertEquals(
                "Best Card visa", cardSuggestionModel.get().get(MAIN_TEXT_CONTENT_DESCRIPTION));
    }

    @Test
    public void testCreatesValidDefaultIbanModel() {
        assertNotNull(mTouchToFillPaymentMethodModel.get(SHEET_ITEMS));
        assertNotNull(mTouchToFillPaymentMethodModel.get(DISMISS_HANDLER));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(false));

        mCoordinator.showIbans(List.of(LOCAL_IBAN));

        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));
    }

    @Test
    public void testScanNewCardNotShownForIbans() {
        mCoordinator.showIbans(List.of(LOCAL_IBAN));
        int lastItemPos = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS).size() - 1;

        assertNull(
                mTouchToFillPaymentMethodModel
                        .get(SHEET_ITEMS)
                        .get(lastItemPos)
                        .model
                        .get(SCAN_CREDIT_CARD_CALLBACK));
    }

    @Test
    public void testShowIbansWithOneEntry() throws TimeoutException {
        mCoordinator.showIbans(List.of(LOCAL_IBAN));

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_IBANS_SHOWN, 1));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, IBAN).size(), is(1));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.fre_product_logo));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_payment_method_bottom_sheet_title));

        Optional<PropertyModel> ibanModel = getIbanModelByAutofillName(itemList, LOCAL_IBAN);
        assertTrue(ibanModel.isPresent());
        assertThat(ibanModel.get().get(IBAN_VALUE), is(LOCAL_IBAN.getLabel()));
        assertThat(ibanModel.get().get(IBAN_NICKNAME), is(LOCAL_IBAN.getNickname()));
    }

    @Test
    public void testShowIbansWithTwoEntries() throws TimeoutException {
        mCoordinator.showIbans(List.of(LOCAL_IBAN, LOCAL_IBAN_NO_NICKNAME));

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_IBANS_SHOWN, 2));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, IBAN).size(), is(2));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.fre_product_logo));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_payment_method_bottom_sheet_title));

        Optional<PropertyModel> ibanModel = getIbanModelByAutofillName(itemList, LOCAL_IBAN);
        assertTrue(ibanModel.isPresent());
        assertThat(ibanModel.get().get(IBAN_VALUE), is(LOCAL_IBAN.getLabel()));
        assertThat(ibanModel.get().get(IBAN_NICKNAME), is(LOCAL_IBAN.getNickname()));

        ibanModel = getIbanModelByAutofillName(itemList, LOCAL_IBAN_NO_NICKNAME);
        assertThat(ibanModel.get().get(IBAN_VALUE), is(LOCAL_IBAN_NO_NICKNAME.getLabel()));
        assertThat(ibanModel.get().get(IBAN_NICKNAME), is(LOCAL_IBAN_NO_NICKNAME.getNickname()));
    }

    @Test
    public void testShowPaymentMethodSettingsForIbans() {
        mCoordinator.showIbans(List.of(LOCAL_IBAN, LOCAL_IBAN_NO_NICKNAME));
        int lastItemPos = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS).size() - 1;
        mTouchToFillPaymentMethodModel
                .get(SHEET_ITEMS)
                .get(lastItemPos)
                .model
                .get(SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK)
                .run();
        verify(mDelegateMock).showPaymentMethodSettings();
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_IBAN_OUTCOME_HISTOGRAM,
                        TouchToFillIbanOutcome.MANAGE_PAYMENTS));
    }

    @Test
    public void testCallsDelegateForIbanOnSelectingItem() {
        mCoordinator.showIbans(List.of(LOCAL_IBAN));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        Optional<PropertyModel> ibanModel =
                getIbanModelByAutofillName(
                        mTouchToFillPaymentMethodModel.get(SHEET_ITEMS), LOCAL_IBAN);
        assertTrue(ibanModel.isPresent());
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        ibanModel.get().get(ON_IBAN_CLICK_ACTION).run();
        verify(mDelegateMock).localIbanSuggestionSelected(LOCAL_IBAN.getGuid());
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_IBAN_OUTCOME_HISTOGRAM, TouchToFillIbanOutcome.IBAN));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_IBAN_INDEX_SELECTED, 0));
    }

    @Test
    public void testShowsContinueButtonWhenOneIban() {
        mCoordinator.showIbans(List.of(LOCAL_IBAN));

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(1, getModelsOfType(itemList, FILL_BUTTON).size());
    }

    @Test
    public void testNoContinueButtonWhenManyIbans() {
        mCoordinator.showIbans(List.of(LOCAL_IBAN, LOCAL_IBAN_NO_NICKNAME));

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(0, getModelsOfType(itemList, FILL_BUTTON).size());
    }

    @Test
    public void testShowOneLoyaltyCard() throws TimeoutException {
        mCoordinator.showLoyaltyCards(List.of(LOYALTY_CARD_1));

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.ic_globe_24dp));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_loyalty_card_bottom_sheet_title));

        assertThat(getModelsOfType(itemList, LOYALTY_CARD).size(), is(1));
        PropertyModel loyaltyCardModel = itemList.get(1).model;
        assertThat(
                loyaltyCardModel.get(LOYALTY_CARD_NUMBER),
                is(LOYALTY_CARD_1.getLoyaltyCardNumber()));
        assertThat(loyaltyCardModel.get(MERCHANT_NAME), is(LOYALTY_CARD_1.getMerchantName()));

        assertThat(getModelsOfType(itemList, FILL_BUTTON).size(), is(1));
    }

    @Test
    public void testShowTwoLoyaltyCards() throws TimeoutException {
        mCoordinator.showLoyaltyCards(List.of(LOYALTY_CARD_1, LOYALTY_CARD_2));

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.ic_globe_24dp));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_loyalty_card_bottom_sheet_title));

        assertThat(getModelsOfType(itemList, LOYALTY_CARD).size(), is(2));
        PropertyModel loyaltyCardModel1 = itemList.get(1).model;
        assertThat(
                loyaltyCardModel1.get(LOYALTY_CARD_NUMBER),
                is(LOYALTY_CARD_1.getLoyaltyCardNumber()));
        assertThat(loyaltyCardModel1.get(MERCHANT_NAME), is(LOYALTY_CARD_1.getMerchantName()));

        PropertyModel loyaltyCardModel2 = itemList.get(2).model;
        assertThat(
                loyaltyCardModel2.get(LOYALTY_CARD_NUMBER),
                is(LOYALTY_CARD_2.getLoyaltyCardNumber()));
        assertThat(loyaltyCardModel2.get(MERCHANT_NAME), is(LOYALTY_CARD_2.getMerchantName()));

        assertThat(getModelsOfType(itemList, FILL_BUTTON).size(), is(0));
    }

    @Test
    public void testCallsDelegateWhenLoyaltyCardIsSelected() {
        mCoordinator.showLoyaltyCards(List.of(LOYALTY_CARD_1));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, LOYALTY_CARD).size(), is(1));

        PropertyModel loyaltyCardModel = itemList.get(1).model;
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        loyaltyCardModel.get(ON_LOYALTY_CARD_CLICK_ACTION).run();
        verify(mDelegateMock).loyaltyCardSuggestionSelected(LOYALTY_CARD_1.getLoyaltyCardNumber());
    }

    private static List<PropertyModel> getModelsOfType(ModelList items, int type) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(item -> item.type == type)
                .map(item -> item.model)
                .collect(Collectors.toList());
    }

    private static Optional<PropertyModel> getCardSuggestionModel(
            ModelList items, AutofillSuggestion suggestion) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(
                        item ->
                                item.type == CREDIT_CARD
                                        && item.model.get(MAIN_TEXT).equals(suggestion.getLabel())
                                        && item.model
                                                .get(MINOR_TEXT)
                                                .equals(suggestion.getSecondaryLabel())
                                        && item.model
                                                .get(FIRST_LINE_LABEL)
                                                .equals(suggestion.getSublabel())
                                        && (TextUtils.isEmpty(suggestion.getSecondarySublabel())
                                                || item.model
                                                        .get(SECOND_LINE_LABEL)
                                                        .equals(suggestion.getSecondarySublabel())))
                .findFirst()
                .map(item -> item.model);
    }

    private static Optional<PropertyModel> getIbanModelByAutofillName(ModelList items, Iban iban) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(
                        item ->
                                item.type == IBAN
                                        && item.model.get(IBAN_VALUE).equals(iban.getLabel()))
                .findFirst()
                .map(item -> item.model);
    }

    private void advanceClockAndClick(PropertyModel cardSuggestionModel) {
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        cardSuggestionModel.get(ON_CREDIT_CARD_CLICK_ACTION).run();
    }
}

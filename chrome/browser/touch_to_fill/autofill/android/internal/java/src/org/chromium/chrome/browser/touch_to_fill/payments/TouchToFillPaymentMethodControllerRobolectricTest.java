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
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MINOR_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.NETWORK_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.ON_CREDIT_CARD_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.SECOND_LINE_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SCAN_CREDIT_CARD_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_NICKNAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_VALUE;
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

import android.app.Activity;
import android.content.Context;
import android.text.TextUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TouchToFillCreditCardOutcome;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TouchToFillIbanOutcome;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.payments.InputProtector;
import org.chromium.components.payments.test_support.FakeClock;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

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
@DisableFeatures({
    AutofillFeatures.AUTOFILL_ENABLE_CARD_ART_IMAGE,
    AutofillFeatures.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID
})
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
    private static final AutofillSuggestion VISA_SUGGESTION =
            createCreditCardSuggestion(
                    VISA.getCardNameForAutofillDisplay(),
                    VISA.getObfuscatedLastFourDigits(),
                    VISA.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false);
    private static final AutofillSuggestion VISA_SUGGESTION_WITH_CARD_BENEFITS =
            createCreditCardSuggestion(
                    VISA.getCardNameForAutofillDisplay(),
                    VISA.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "2% cashback on travel",
                    VISA.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ true);
    private static final AutofillSuggestion NICKNAMED_VISA_SUGGESTION =
            createCreditCardSuggestion(
                    NICKNAMED_VISA.getCardNameForAutofillDisplay(),
                    NICKNAMED_VISA.getObfuscatedLastFourDigits(),
                    NICKNAMED_VISA.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false);
    private static final AutofillSuggestion MASTERCARD_SUGGESTION =
            createCreditCardSuggestion(
                    MASTERCARD.getCardNameForAutofillDisplay(),
                    MASTERCARD.getObfuscatedLastFourDigits(),
                    MASTERCARD.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false);
    private static final AutofillSuggestion NON_ACCEPTABLE_VIRTUAL_CARD_SUGGESTION =
            createCreditCardSuggestion(
                    VIRTUAL_CARD.getCardNameForAutofillDisplay(),
                    VIRTUAL_CARD.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "Merchant doesn't accept this virtual card",
                    /* secondarySubLabel= */ "",
                    /* applyDeactivatedStyle= */ true,
                    /* shouldDisplayTermsAvailable= */ false);
    private static final AutofillSuggestion ACCEPTABLE_VIRTUAL_CARD_SUGGESTION =
            createCreditCardSuggestion(
                    VIRTUAL_CARD.getCardNameForAutofillDisplay(),
                    VIRTUAL_CARD.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "Virtual Card",
                    /* secondarySubLabel= */ "",
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false);
    private static final AutofillSuggestion VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS =
            createCreditCardSuggestion(
                    VIRTUAL_CARD.getCardNameForAutofillDisplay(),
                    VIRTUAL_CARD.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "2% cashback on travel",
                    /* secondarySubLabel= */ "Virtual card",
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ true);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private TouchToFillPaymentMethodCoordinator mCoordinator;
    private PropertyModel mTouchToFillPaymentMethodModel;
    private FakeClock mClock = new FakeClock();
    Context mContext;

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TouchToFillPaymentMethodComponent.Delegate mDelegateMock;
    @Mock private BottomSheetFocusHelper mBottomSheetFocusHelper;
    @Mock private PersonalDataManager mPersonalDataManager;

    public TouchToFillPaymentMethodControllerRobolectricTest() {
        mCoordinator = new TouchToFillPaymentMethodCoordinator();
        mContext = Robolectric.buildActivity(Activity.class).get();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Mockito.when(
                        mBottomSheetController.requestShowContent(
                                any(BottomSheetContent.class), anyBoolean()))
                .thenReturn(true);
        mCoordinator.initialize(
                mContext,
                mPersonalDataManager,
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
        mCoordinator.showSheet(
                List.of(VISA), List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);

        verify(mBottomSheetFocusHelper, times(1)).registerForOneTimeUse();
    }

    @Test
    public void testCreatesValidDefaultCreditCardModel() {
        assertNotNull(mTouchToFillPaymentMethodModel.get(SHEET_ITEMS));
        assertNotNull(mTouchToFillPaymentMethodModel.get(DISMISS_HANDLER));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(false));

        mCoordinator.showSheet(
                List.of(VISA), List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);

        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));
    }

    @Test
    public void testShowCreditCardSuggestionsWithOneEntry() throws TimeoutException {
        mCoordinator.showSheet(
                List.of(VISA), List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, 1));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(1));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));

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
        mCoordinator.showSheet(
                List.of(VISA, MASTERCARD),
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ false);

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, 2));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(2));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));

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

        mCoordinator.showSheet(
                List.of(VIRTUAL_CARD, MASTERCARD),
                List.of(NON_ACCEPTABLE_VIRTUAL_CARD_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ false);

        metricsWatcher.assertExpected();

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(2));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, NON_ACCEPTABLE_VIRTUAL_CARD_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertTrue(cardSuggestionModel.get().get(APPLY_DEACTIVATED_STYLE));

        cardSuggestionModel = getCardSuggestionModel(itemList, MASTERCARD_SUGGESTION);
        assertFalse(cardSuggestionModel.get().get(APPLY_DEACTIVATED_STYLE));
    }

    @Test
    public void testShowCreditCardSuggestionsWithCardBenefits() throws TimeoutException {
        mCoordinator.showSheet(
                List.of(MASTERCARD, VISA, VIRTUAL_CARD),
                List.of(
                        MASTERCARD_SUGGESTION,
                        VISA_SUGGESTION_WITH_CARD_BENEFITS,
                        VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS),
                /* shouldShowScanCreditCard= */ false);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(3));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));

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
        mCoordinator.showSheet(
                List.of(VISA),
                List.of(VISA_SUGGESTION_WITH_CARD_BENEFITS),
                /* shouldShowScanCreditCard= */ true);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        List<PropertyModel> termsLabelModelList = getModelsOfType(itemList, TERMS_LABEL);
        assertEquals(termsLabelModelList.size(), 1);
        assertTrue(termsLabelModelList.get(0).get(CARD_BENEFITS_TERMS_AVAILABLE));
    }

    @Test
    public void testBenefitsTermsLabel_HiddenWhenCardHasNoBenefits() {
        mCoordinator.showSheet(
                List.of(VISA), List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ true);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(getModelsOfType(itemList, TERMS_LABEL).size(), 0);
    }

    @Test
    public void testScanNewCardIsShownForCreditCards() {
        mCoordinator.showSheet(
                List.of(VISA, MASTERCARD),
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
        mCoordinator.showSheet(
                List.of(VISA, MASTERCARD),
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
        mCoordinator.showSheet(
                List.of(VISA), List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);
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
        mCoordinator.showSheet(
                List.of(VISA), List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);
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
        mCoordinator.showSheet(
                List.of(VIRTUAL_CARD),
                List.of(ACCEPTABLE_VIRTUAL_CARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ false);
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
        mCoordinator.showSheet(
                List.of(VISA), List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ true);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(getModelsOfType(itemList, FILL_BUTTON).size(), 1);
    }

    @Test
    public void testNoContinueButtonWhenManyCreditCards() {
        mCoordinator.showSheet(
                List.of(VISA, MASTERCARD),
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(getModelsOfType(itemList, FILL_BUTTON).size(), 0);
    }

    @Test
    public void testDismissWithSwipeForCreditCard() {
        mCoordinator.showSheet(
                List.of(VISA, MASTERCARD),
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
        mCoordinator.showSheet(
                List.of(VISA, MASTERCARD),
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);

        mTouchToFillPaymentMethodModel.get(DISMISS_HANDLER).onResult(StateChangeReason.TAP_SCRIM);

        metricsWatcher.assertExpected();
    }

    @Test
    public void testScanNewCardClick() {
        mCoordinator.showSheet(
                List.of(VISA, MASTERCARD),
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        getModelsOfType(itemList, FOOTER).get(0).get(SCAN_CREDIT_CARD_CALLBACK).run();

        verify(mDelegateMock).scanCreditCard();
    }

    @Test
    public void testManagePaymentMethodsClickForCreditCard() {
        mCoordinator.showSheet(
                List.of(VISA, MASTERCARD),
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ false);
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        getModelsOfType(itemList, FOOTER).get(0).get(SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK).run();

        verify(mDelegateMock).showPaymentMethodSettings();
    }

    @Test
    public void testContinueButtonClickForCreditCard() {
        mCoordinator.showSheet(
                List.of(VISA), List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        advanceClockAndClick(getModelsOfType(itemList, FILL_BUTTON).get(0));
        verify(mDelegateMock).creditCardSuggestionSelected(VISA.getGUID(), VISA.getIsVirtual());
    }

    @Test
    public void testCardSuggestionModelForNicknamedCardContainsANetworkName() {
        mCoordinator.showSheet(
                List.of(NICKNAMED_VISA),
                List.of(NICKNAMED_VISA_SUGGESTION),
                /* shouldShowScanCreditCard= */ false);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, NICKNAMED_VISA_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertEquals("visa", cardSuggestionModel.get().get(NETWORK_NAME));
    }

    @Test
    public void testCardSuggestionModelForACardWithoutANicknameDoesNotContainANetworkName() {
        mCoordinator.showSheet(
                List.of(VISA), List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, VISA_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertTrue(cardSuggestionModel.get().get(NETWORK_NAME).isEmpty());
    }

    @Test
    public void testCreatesValidDefaultIbanModel() {
        assertNotNull(mTouchToFillPaymentMethodModel.get(SHEET_ITEMS));
        assertNotNull(mTouchToFillPaymentMethodModel.get(DISMISS_HANDLER));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(false));

        mCoordinator.showSheet(List.of(LOCAL_IBAN));

        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));
    }

    @Test
    public void testScanNewCardNotShownForIbans() {
        mCoordinator.showSheet(List.of(LOCAL_IBAN));
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
        mCoordinator.showSheet(List.of(LOCAL_IBAN));

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_IBANS_SHOWN, 1));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, IBAN).size(), is(1));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));

        Optional<PropertyModel> ibanModel = getIbanModelByAutofillName(itemList, LOCAL_IBAN);
        assertTrue(ibanModel.isPresent());
        assertThat(ibanModel.get().get(IBAN_VALUE), is(LOCAL_IBAN.getLabel()));
        assertThat(ibanModel.get().get(IBAN_NICKNAME), is(LOCAL_IBAN.getNickname()));
    }

    @Test
    public void testShowIbansWithTwoEntries() throws TimeoutException {
        mCoordinator.showSheet(List.of(LOCAL_IBAN, LOCAL_IBAN_NO_NICKNAME));

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_IBANS_SHOWN, 2));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, IBAN).size(), is(2));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));

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
        mCoordinator.showSheet(List.of(LOCAL_IBAN, LOCAL_IBAN_NO_NICKNAME));
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
        mCoordinator.showSheet(List.of(LOCAL_IBAN));
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
        mCoordinator.showSheet(List.of(LOCAL_IBAN));

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(getModelsOfType(itemList, FILL_BUTTON).size(), 1);
    }

    @Test
    public void testNoContinueButtonWhenManyIbans() {
        mCoordinator.showSheet(List.of(LOCAL_IBAN, LOCAL_IBAN_NO_NICKNAME));

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(getModelsOfType(itemList, FILL_BUTTON).size(), 0);
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

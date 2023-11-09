// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createCreditCard;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createVirtualCreditCard;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardMediator.TOUCH_TO_FILL_INDEX_SELECTED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardMediator.TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardMediator.TOUCH_TO_FILL_OUTCOME_HISTOGRAM;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardMediator.TOUCH_TO_FILL_OUTCOME_HISTOGRAM_FIXED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.CARD_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.CARD_NUMBER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.NETWORK_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.ON_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.FooterProperties.SCAN_CREDIT_CARD_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.FooterProperties.SHOW_CREDIT_CARD_SETTINGS_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.FOOTER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.HEADER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.VISIBLE;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardMediator.TouchToFillCreditCardOutcome;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.autofill.AutofillFeatures;
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

/** Tests for {@link TouchToFillCreditCardCoordinator} and {@link TouchToFillCreditCardMediator} */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
@DisableFeatures(AutofillFeatures.AUTOFILL_ENABLE_CARD_ART_IMAGE)
public class TouchToFillCreditCardControllerRobolectricTest {
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
    private static final CreditCard MASTER_CARD =
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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    private TouchToFillCreditCardCoordinator mCoordinator;
    private PropertyModel mTouchToFillCreditCardModel;
    private FakeClock mClock = new FakeClock();
    Context mContext;

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TouchToFillCreditCardComponent.Delegate mDelegateMock;
    @Mock private BottomSheetFocusHelper mBottomSheetFocusHelper;

    public TouchToFillCreditCardControllerRobolectricTest() {
        mCoordinator = new TouchToFillCreditCardCoordinator();
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
                mContext, mBottomSheetController, mDelegateMock, mBottomSheetFocusHelper);
        mTouchToFillCreditCardModel = mCoordinator.getModelForTesting();
        mCoordinator
                .getMediatorForTesting()
                .setInputProtectorForTesting(new InputProtector(mClock));
    }

    @Test
    public void testAddsTheBottomSheetHelperToObserveTheSheet() {
        mCoordinator.showSheet(new CreditCard[] {VISA}, false);

        verify(mBottomSheetFocusHelper, times(1)).registerForOneTimeUse();
    }

    @Test
    public void testCreatesValidDefaultModel() {
        assertNotNull(mTouchToFillCreditCardModel.get(SHEET_ITEMS));
        assertNotNull(mTouchToFillCreditCardModel.get(DISMISS_HANDLER));
        assertThat(mTouchToFillCreditCardModel.get(VISIBLE), is(false));

        mCoordinator.showSheet(new CreditCard[] {VISA}, false);

        assertThat(mTouchToFillCreditCardModel.get(VISIBLE), is(true));
    }

    @Test
    @EnableFeatures(AutofillFeatures.AUTOFILL_TOUCH_TO_FILL_FOR_CREDIT_CARDS_ANDROID)
    public void testShowCreditCardsWithOneEntry() throws TimeoutException {
        mCoordinator.showSheet(new CreditCard[] {VISA}, false);

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, 1));
        ModelList itemList = mTouchToFillCreditCardModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(1));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));

        Optional<PropertyModel> cardModel = getCardModelByAutofillName(itemList, VISA);
        assertTrue(cardModel.isPresent());
        assertThat(cardModel.get().get(CARD_NAME), is(VISA.getCardNameForAutofillDisplay()));
        assertThat(cardModel.get().get(CARD_NUMBER), is(VISA.getObfuscatedLastFourDigits()));
    }

    @Test
    @EnableFeatures(AutofillFeatures.AUTOFILL_TOUCH_TO_FILL_FOR_CREDIT_CARDS_ANDROID)
    public void testShowCreditCardsWithTwoEntries() throws TimeoutException {
        mCoordinator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, false);

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, 2));
        ModelList itemList = mTouchToFillCreditCardModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(2));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));

        Optional<PropertyModel> cardModel = getCardModelByAutofillName(itemList, VISA);
        assertTrue(cardModel.isPresent());
        assertThat(cardModel.get().get(CARD_NAME), is(VISA.getCardNameForAutofillDisplay()));
        assertThat(cardModel.get().get(CARD_NUMBER), is(VISA.getObfuscatedLastFourDigits()));

        cardModel = getCardModelByAutofillName(itemList, MASTER_CARD);
        assertThat(cardModel.get().get(CARD_NAME), is(MASTER_CARD.getCardNameForAutofillDisplay()));
        assertThat(cardModel.get().get(CARD_NUMBER), is(MASTER_CARD.getObfuscatedLastFourDigits()));
    }

    @Test
    public void testScanNewCard() {
        mCoordinator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, true);
        int lastItemPos = mTouchToFillCreditCardModel.get(SHEET_ITEMS).size() - 1;
        mTouchToFillCreditCardModel
                .get(SHEET_ITEMS)
                .get(lastItemPos)
                .model
                .get(SCAN_CREDIT_CARD_CALLBACK)
                .run();
        verify(mDelegateMock).scanCreditCard();
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.SCAN_NEW_CARD));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_OUTCOME_HISTOGRAM_FIXED,
                        TouchToFillCreditCardOutcome.SCAN_NEW_CARD));
    }

    @Test
    public void testShowCreditCardSettings() {
        mCoordinator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, true);
        int lastItemPos = mTouchToFillCreditCardModel.get(SHEET_ITEMS).size() - 1;
        mTouchToFillCreditCardModel
                .get(SHEET_ITEMS)
                .get(lastItemPos)
                .model
                .get(SHOW_CREDIT_CARD_SETTINGS_CALLBACK)
                .run();
        verify(mDelegateMock).showCreditCardSettings();
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.MANAGE_PAYMENTS));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_OUTCOME_HISTOGRAM_FIXED,
                        TouchToFillCreditCardOutcome.MANAGE_PAYMENTS));
    }

    @Test
    @EnableFeatures(AutofillFeatures.AUTOFILL_TOUCH_TO_FILL_FOR_CREDIT_CARDS_ANDROID)
    public void testNoCallbackForCreditCardOnSelectingItemBeforeInputTime() {
        mCoordinator.showSheet(new CreditCard[] {VISA}, false);
        assertThat(mTouchToFillCreditCardModel.get(VISIBLE), is(true));

        Optional<PropertyModel> cardModel =
                getCardModelByAutofillName(mTouchToFillCreditCardModel.get(SHEET_ITEMS), VISA);
        assertNotNull(cardModel.get().get(ON_CLICK_ACTION));

        // Clicking after an interval less than the threshold should be a no-op.
        mClock.advanceCurrentTimeMillis(
                InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD - 100);
        cardModel.get().get(ON_CLICK_ACTION).run();
        verify(mDelegateMock, times(0)).suggestionSelected(VISA.getGUID(), VISA.getIsVirtual());

        // Clicking after the threshold should work.
        mClock.advanceCurrentTimeMillis(100);
        cardModel.get().get(ON_CLICK_ACTION).run();
        verify(mDelegateMock, times(1)).suggestionSelected(VISA.getGUID(), VISA.getIsVirtual());
    }

    @Test
    @EnableFeatures(AutofillFeatures.AUTOFILL_TOUCH_TO_FILL_FOR_CREDIT_CARDS_ANDROID)
    public void testCallsCallbackForCreditCardOnSelectingItem() {
        mCoordinator.showSheet(new CreditCard[] {VISA}, false);
        assertThat(mTouchToFillCreditCardModel.get(VISIBLE), is(true));

        Optional<PropertyModel> cardModel =
                getCardModelByAutofillName(mTouchToFillCreditCardModel.get(SHEET_ITEMS), VISA);
        assertNotNull(cardModel.get().get(ON_CLICK_ACTION));

        advanceClockAndClick(cardModel.get());
        verify(mDelegateMock).suggestionSelected(VISA.getGUID(), VISA.getIsVirtual());
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_OUTCOME_HISTOGRAM, TouchToFillCreditCardOutcome.CREDIT_CARD));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_OUTCOME_HISTOGRAM_FIXED,
                        TouchToFillCreditCardOutcome.CREDIT_CARD));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(TOUCH_TO_FILL_INDEX_SELECTED, 0));
    }

    @Test
    @EnableFeatures(AutofillFeatures.AUTOFILL_TOUCH_TO_FILL_FOR_CREDIT_CARDS_ANDROID)
    public void testCallsCallbackForVirtualCardOnSelectingItem() {
        mCoordinator.showSheet(new CreditCard[] {VIRTUAL_CARD}, false);
        assertThat(mTouchToFillCreditCardModel.get(VISIBLE), is(true));

        Optional<PropertyModel> cardModel =
                getCardModelByAutofillName(
                        mTouchToFillCreditCardModel.get(SHEET_ITEMS), VIRTUAL_CARD);
        assertNotNull(cardModel.get().get(ON_CLICK_ACTION));

        advanceClockAndClick(cardModel.get());
        verify(mDelegateMock)
                .suggestionSelected(VIRTUAL_CARD.getGUID(), VIRTUAL_CARD.getIsVirtual());
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.VIRTUAL_CARD));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_OUTCOME_HISTOGRAM_FIXED,
                        TouchToFillCreditCardOutcome.VIRTUAL_CARD));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(TOUCH_TO_FILL_INDEX_SELECTED, 0));
    }

    @Test
    public void testShowsContinueButtonWhenOneItem() {
        mCoordinator.showSheet(new CreditCard[] {VISA}, true);

        ModelList itemList = mTouchToFillCreditCardModel.get(SHEET_ITEMS);
        assertEquals(getModelsOfType(itemList, FILL_BUTTON).size(), 1);
    }

    @Test
    public void testNoContinueButtonWhenManyItems() {
        mCoordinator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, true);

        ModelList itemList = mTouchToFillCreditCardModel.get(SHEET_ITEMS);
        assertEquals(getModelsOfType(itemList, FILL_BUTTON).size(), 0);
    }

    @Test
    public void testDismissWithSwipe() {
        mCoordinator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, true);

        mTouchToFillCreditCardModel.get(DISMISS_HANDLER).onResult(StateChangeReason.SWIPE);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_OUTCOME_HISTOGRAM, TouchToFillCreditCardOutcome.DISMISS));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_OUTCOME_HISTOGRAM_FIXED,
                        TouchToFillCreditCardOutcome.DISMISS));
    }

    @Test
    public void testDismissWithTap() {
        HistogramWatcher metricsWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TOUCH_TO_FILL_OUTCOME_HISTOGRAM_FIXED,
                        TouchToFillCreditCardOutcome.DISMISS);
        mCoordinator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, true);

        mTouchToFillCreditCardModel.get(DISMISS_HANDLER).onResult(StateChangeReason.TAP_SCRIM);

        metricsWatcher.assertExpected();
    }

    @Test
    public void testScanNewCardClick() {
        mCoordinator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, true);
        ModelList itemList = mTouchToFillCreditCardModel.get(SHEET_ITEMS);
        getModelsOfType(itemList, FOOTER).get(0).get(SCAN_CREDIT_CARD_CALLBACK).run();

        verify(mDelegateMock).scanCreditCard();
    }

    @Test
    public void testManagePaymentMethodsClick() {
        mCoordinator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, false);
        ModelList itemList = mTouchToFillCreditCardModel.get(SHEET_ITEMS);
        getModelsOfType(itemList, FOOTER).get(0).get(SHOW_CREDIT_CARD_SETTINGS_CALLBACK).run();

        verify(mDelegateMock).showCreditCardSettings();
    }

    @Test
    public void testContinueButtonClick() {
        mCoordinator.showSheet(new CreditCard[] {VISA}, false);
        ModelList itemList = mTouchToFillCreditCardModel.get(SHEET_ITEMS);
        advanceClockAndClick(getModelsOfType(itemList, FILL_BUTTON).get(0));
        verify(mDelegateMock).suggestionSelected(VISA.getGUID(), VISA.getIsVirtual());
    }

    @Test
    public void testCardModelForNicknamedCardContainsANetworkName() {
        mCoordinator.showSheet(new CreditCard[] {NICKNAMED_VISA}, false);

        ModelList itemList = mTouchToFillCreditCardModel.get(SHEET_ITEMS);

        Optional<PropertyModel> cardModel = getCardModelByAutofillName(itemList, NICKNAMED_VISA);
        assertTrue(cardModel.isPresent());
        assertEquals("visa", cardModel.get().get(NETWORK_NAME));
    }

    @Test
    public void testCardModelForACardWithoutANicknameDoesNotContainANetworkName() {
        mCoordinator.showSheet(new CreditCard[] {VISA}, false);

        ModelList itemList = mTouchToFillCreditCardModel.get(SHEET_ITEMS);

        Optional<PropertyModel> cardModel = getCardModelByAutofillName(itemList, VISA);
        assertTrue(cardModel.isPresent());
        assertTrue(cardModel.get().get(NETWORK_NAME).isEmpty());
    }

    private static List<PropertyModel> getModelsOfType(ModelList items, int type) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(item -> item.type == type)
                .map(item -> item.model)
                .collect(Collectors.toList());
    }

    private static Optional<PropertyModel> getCardModelByAutofillName(
            ModelList items, CreditCard card) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(
                        item ->
                                item.type == CREDIT_CARD
                                        && item.model
                                                .get(CARD_NAME)
                                                .equals(card.getCardNameForAutofillDisplay()))
                .findFirst()
                .map(item -> item.model);
    }

    private void advanceClockAndClick(PropertyModel cardModel) {
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        cardModel.get(ON_CLICK_ACTION).run();
    }
}

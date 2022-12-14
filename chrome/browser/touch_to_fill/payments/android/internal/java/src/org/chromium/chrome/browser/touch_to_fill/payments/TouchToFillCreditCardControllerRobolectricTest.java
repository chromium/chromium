// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createCreditCard;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.CARD_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.CARD_NUMBER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.ON_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.HEADER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.SCAN_CREDIT_CARD_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.SHOW_CREDIT_CARD_SETTINGS_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.VISIBLE;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
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
public class TouchToFillCreditCardControllerRobolectricTest {
    private static final CreditCard VISA = createCreditCard(
            "Visa", "4111111111111111", "5", "2050", true, "Visa", "• • • • 1111", 0);
    private static final CreditCard MASTER_CARD = createCreditCard(
            "MasterCard", "5555555555554444", "8", "2050", true, "MasterCard", "• • • • 4444", 0);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private TouchToFillCreditCardCoordinator mCoordinator;
    private PropertyModel mTouchToFillCreditCardModel;
    Context mContext;

    @Mock
    private BottomSheetController mBottomSheetController;

    @Mock
    TouchToFillCreditCardComponent.Delegate mDelegateMock;

    public TouchToFillCreditCardControllerRobolectricTest() {
        mCoordinator = new TouchToFillCreditCardCoordinator();
        mContext = Robolectric.buildActivity(Activity.class).get();
    }

    @Before
    public void setUp() {
        Mockito.when(mBottomSheetController.requestShowContent(
                             any(BottomSheetContent.class), anyBoolean()))
                .thenReturn(true);

        mCoordinator.initialize(mContext, mBottomSheetController, mDelegateMock);
        mTouchToFillCreditCardModel = mCoordinator.getModelForTesting();
    }

    @Before
    public void testCreatesValidDefaultModel() {
        assertNotNull(mTouchToFillCreditCardModel.get(SHEET_ITEMS));
        assertNotNull(mTouchToFillCreditCardModel.get(DISMISS_HANDLER));
        assertThat(mTouchToFillCreditCardModel.get(VISIBLE), is(false));
    }

    @Test
    @EnableFeatures({AutofillFeatures.AUTOFILL_TOUCH_TO_FILL_FOR_CREDIT_CARDS_ANDROID})
    public void testShowCreditCardsWithOneEntry() throws TimeoutException {
        mCoordinator.showSheet(new CreditCard[] {VISA}, false);

        ModelList itemList = mTouchToFillCreditCardModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(1));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));

        Optional<PropertyModel> cardModel = getCardModelByAutofillName(itemList, VISA);
        assertTrue(cardModel.isPresent());
        assertThat(cardModel.get().get(CARD_NAME), is(VISA.getCardNameForAutofillDisplay()));
        assertThat(cardModel.get().get(CARD_NUMBER), is(VISA.getObfuscatedLastFourDigits()));
    }

    @Test
    @EnableFeatures({AutofillFeatures.AUTOFILL_TOUCH_TO_FILL_FOR_CREDIT_CARDS_ANDROID})
    public void testShowCreditCardsWithTwoEntries() throws TimeoutException {
        mCoordinator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, false);

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
        mTouchToFillCreditCardModel.get(SCAN_CREDIT_CARD_CALLBACK).run();
        verify(mDelegateMock).scanCreditCard();
    }

    @Test
    public void testShowCreditCardSettings() {
        mCoordinator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, true);
        mTouchToFillCreditCardModel.get(SHOW_CREDIT_CARD_SETTINGS_CALLBACK).run();
        verify(mDelegateMock).showCreditCardSettings();
    }

    @Test
    @EnableFeatures({AutofillFeatures.AUTOFILL_TOUCH_TO_FILL_FOR_CREDIT_CARDS_ANDROID,
            AutofillFeatures.AUTOFILL_ACROSS_IFRAMES})
    public void
    testCallsCallbackOnSelectingItem() {
        mCoordinator.showSheet(new CreditCard[] {VISA}, false);
        assertThat(mTouchToFillCreditCardModel.get(VISIBLE), is(true));

        Optional<PropertyModel> cardModel =
                getCardModelByAutofillName(mTouchToFillCreditCardModel.get(SHEET_ITEMS), VISA);
        assertNotNull(cardModel.get().get(ON_CLICK_ACTION));

        cardModel.get().get(ON_CLICK_ACTION).run();
        verify(mDelegateMock).suggestionSelected(VISA.getGUID());
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

    private static List<PropertyModel> getModelsOfType(ModelList items, int type) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(item -> item.type == type)
                .map(item -> item.model)
                .collect(Collectors.toList());
    }

    private static Optional<PropertyModel> getCardModelByAutofillName(
            ModelList items, CreditCard card) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(item
                        -> item.type == CREDIT_CARD
                                && item.model.get(CARD_NAME).equals(
                                        card.getCardNameForAutofillDisplay()))
                .findFirst()
                .map(item -> item.model);
    }
}

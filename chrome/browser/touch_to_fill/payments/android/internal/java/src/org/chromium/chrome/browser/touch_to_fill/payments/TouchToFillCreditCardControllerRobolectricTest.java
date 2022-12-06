// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createLocalCreditCard;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.CARD_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.CARD_NUMBER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.ON_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.CREDIT_CARD;
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
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeoutException;

/** Tests for {@link TouchToFillCreditCardCoordinator} and {@link TouchToFillCreditCardMediator} */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class TouchToFillCreditCardControllerRobolectricTest {
    private static final CreditCard VISA =
            createLocalCreditCard("Visa", "4111111111111111", "5", "2050");
    private static final CreditCard MASTER_CARD =
            createLocalCreditCard("MasterCard", "5555555555554444", "8", "2050");

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private TouchToFillCreditCardCoordinator mCoordinator;
    private PropertyModel mTouchToFillCreditCardModel;
    private final TouchToFillCreditCardMediator mMediator = new TouchToFillCreditCardMediator();
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
        MockitoAnnotations.initMocks(this);
        mCoordinator.initialize(mContext, mBottomSheetController, mDelegateMock);
        mTouchToFillCreditCardModel = mCoordinator.createModel(mMediator);
        mMediator.initialize(mContext, mDelegateMock, mTouchToFillCreditCardModel);
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
        mMediator.showSheet(new CreditCard[] {VISA}, false);

        ModelList itemList = mTouchToFillCreditCardModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(1));

        assertThat(itemList.get(0).type, is(CREDIT_CARD));
        assertThat(itemList.get(0).model.get(CARD_NAME), is(VISA.getCardNameForAutofillDisplay()));
        assertThat(itemList.get(0).model.get(CARD_NUMBER), is(VISA.getObfuscatedLastFourDigits()));
    }

    @Test
    @EnableFeatures({AutofillFeatures.AUTOFILL_TOUCH_TO_FILL_FOR_CREDIT_CARDS_ANDROID})
    public void testShowCreditCardsWithTwoEntries() throws TimeoutException {
        mMediator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, false);

        ModelList itemList = mTouchToFillCreditCardModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(2));

        assertThat(itemList.get(0).type, is(CREDIT_CARD));
        assertThat(itemList.get(0).model.get(CARD_NAME), is(VISA.getCardNameForAutofillDisplay()));
        assertThat(itemList.get(0).model.get(CARD_NUMBER), is(VISA.getObfuscatedLastFourDigits()));

        assertThat(itemList.get(1).type, is(CREDIT_CARD));
        assertThat(itemList.get(0).model.get(CARD_NAME),
                is(MASTER_CARD.getCardNameForAutofillDisplay()));
        assertThat(itemList.get(0).model.get(CARD_NUMBER),
                is(MASTER_CARD.getObfuscatedLastFourDigits()));
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
        mMediator.showSheet(new CreditCard[] {VISA}, false);
        assertThat(mTouchToFillCreditCardModel.get(VISIBLE), is(true));
        assertNotNull(
                mTouchToFillCreditCardModel.get(SHEET_ITEMS).get(0).model.get(ON_CLICK_ACTION));

        mTouchToFillCreditCardModel.get(SHEET_ITEMS).get(0).model.get(ON_CLICK_ACTION).run();
        verify(mDelegateMock).suggestionSelected(VISA.getGUID());
    }

    private static PersonalDataManager.CreditCard createLocalCreditCard(
            String name, String number, String month, String year) {
        return new CreditCard("", "", true, false, name, number, "", month, year, "", 0, "", "");
    }
}

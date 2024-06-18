// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.ON_BANK_ACCOUNT_CLICK_ACTION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.ADDITIONAL_INFO;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.BANK_ACCOUNT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.CONTINUE_BUTTON;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.HEADER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE;

import android.app.Activity;
import android.content.Context;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.autofill.payments.AccountType;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.PaymentInstrument;
import org.chromium.components.autofill.payments.PaymentRail;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.payments.InputProtector;
import org.chromium.components.payments.test_support.FakeClock;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Optional;
import java.util.stream.Collectors;
import java.util.stream.StreamSupport;

/**
 * Tests for {@link FacilitatedPaymentsPaymentMethodsCoordinator} and {@link
 * FacilitatedPaymentsPaymentMethodsMediator}
 */
@RunWith(BaseRobolectricTestRunner.class)
public class FacilitatedPaymentsPaymentMethodsControllerRobolectricTest {
    private static final BankAccount BANK_ACCOUNT_1 =
            new BankAccount.Builder()
                    .setPaymentInstrument(
                            new PaymentInstrument.Builder()
                                    .setInstrumentId(100)
                                    .setNickname("nickname1")
                                    .setSupportedPaymentRails(new int[] {PaymentRail.PIX})
                                    .build())
                    .setBankName("bankName1")
                    .setAccountNumberSuffix("1111")
                    .setAccountType(AccountType.CHECKING)
                    .build();
    private static final BankAccount BANK_ACCOUNT_2 =
            new BankAccount.Builder()
                    .setPaymentInstrument(
                            new PaymentInstrument.Builder()
                                    .setInstrumentId(200)
                                    .setNickname("nickname2")
                                    .setSupportedPaymentRails(new int[] {PaymentRail.PIX})
                                    .build())
                    .setBankName("bankName2")
                    .setAccountNumberSuffix("2222")
                    .setAccountType(AccountType.SAVINGS)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    private FacilitatedPaymentsPaymentMethodsCoordinator mCoordinator;
    private PropertyModel mFacilitatedPaymentsPaymentMethodsModel;
    private FakeClock mClock = new FakeClock();
    Context mContext;

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private FacilitatedPaymentsPaymentMethodsComponent.Delegate mDelegateMock;

    public FacilitatedPaymentsPaymentMethodsControllerRobolectricTest() {
        mCoordinator = new FacilitatedPaymentsPaymentMethodsCoordinator();
        mContext = Robolectric.buildActivity(Activity.class).get();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Mockito.when(
                        mBottomSheetController.requestShowContent(
                                any(BottomSheetContent.class), anyBoolean()))
                .thenReturn(true);
        mCoordinator.initialize(mContext, mBottomSheetController, mDelegateMock);
        mFacilitatedPaymentsPaymentMethodsModel = mCoordinator.getModelForTesting();
        mCoordinator
                .getMediatorForTesting()
                .setInputProtectorForTesting(new InputProtector(mClock));
    }

    @Test
    public void testCreatesValidDefaultPropertyModel() {
        assertNotNull(mFacilitatedPaymentsPaymentMethodsModel.get(SHEET_ITEMS));
        ModelList itemList = mFacilitatedPaymentsPaymentMethodsModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(0));
        assertNotNull(mFacilitatedPaymentsPaymentMethodsModel.get(DISMISS_HANDLER));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE), is(false));
    }

    @Test
    public void testBankAccountsShown() {
        assertNotNull(mFacilitatedPaymentsPaymentMethodsModel.get(SHEET_ITEMS));
        ModelList itemList = mFacilitatedPaymentsPaymentMethodsModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(0));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE), is(false));

        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1, BANK_ACCOUNT_2));

        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE), is(true));
        assertThat(itemList.size(), is(4));
        assertEquals(itemList.get(0).type, HEADER);
        assertEquals(itemList.get(1).type, BANK_ACCOUNT);
        assertEquals(itemList.get(2).type, BANK_ACCOUNT);
        assertEquals(itemList.get(3).type, ADDITIONAL_INFO);
    }

    @Test
    public void testOnDismissedIsCalled() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1, BANK_ACCOUNT_2));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE), is(true));

        mFacilitatedPaymentsPaymentMethodsModel
                .get(DISMISS_HANDLER)
                .onResult(StateChangeReason.SWIPE);

        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE), is(false));
        verify(mDelegateMock).onDismissed();
    }

    @Test
    public void testShowsContinueButtonWhenOneBankAccount() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1));

        ModelList itemList = mFacilitatedPaymentsPaymentMethodsModel.get(SHEET_ITEMS);
        assertEquals(getModelsOfType(itemList, CONTINUE_BUTTON).size(), 1);
    }

    @Test
    public void testNoContinueButtonWhenManyBankAccounts() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1, BANK_ACCOUNT_2));

        ModelList itemList = mFacilitatedPaymentsPaymentMethodsModel.get(SHEET_ITEMS);
        assertEquals(getModelsOfType(itemList, CONTINUE_BUTTON).size(), 0);
    }

    @Test
    public void testCallbackIsCalledWhenBankAccountIsSelected() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE), is(true));

        Optional<PropertyModel> bankAccountModel =
                getBankAccountModelByBankName(
                        mFacilitatedPaymentsPaymentMethodsModel.get(SHEET_ITEMS), BANK_ACCOUNT_1);
        assertNotNull(bankAccountModel.get().get(ON_BANK_ACCOUNT_CLICK_ACTION));

        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        bankAccountModel.get().get(ON_BANK_ACCOUNT_CLICK_ACTION).run();
        verify(mDelegateMock).onBankAccountSelected(BANK_ACCOUNT_1.getInstrumentId());
    }

    @Test
    public void testNoCallbackForSelectedBankAccountBeforeInputTime() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE), is(true));

        Optional<PropertyModel> bankAccountModel =
                getBankAccountModelByBankName(
                        mFacilitatedPaymentsPaymentMethodsModel.get(SHEET_ITEMS), BANK_ACCOUNT_1);
        assertNotNull(bankAccountModel.get().get(ON_BANK_ACCOUNT_CLICK_ACTION));

        // Clicking after an interval less than the threshold should be a no-op.
        mClock.advanceCurrentTimeMillis(
                InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD - 100);
        bankAccountModel.get().get(ON_BANK_ACCOUNT_CLICK_ACTION).run();
        verify(mDelegateMock, times(0)).onBankAccountSelected(BANK_ACCOUNT_1.getInstrumentId());

        // Clicking after the threshold should work.
        mClock.advanceCurrentTimeMillis(200);
        bankAccountModel.get().get(ON_BANK_ACCOUNT_CLICK_ACTION).run();
        verify(mDelegateMock, times(1)).onBankAccountSelected(BANK_ACCOUNT_1.getInstrumentId());
    }

    private static List<PropertyModel> getModelsOfType(ModelList items, int type) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(item -> item.type == type)
                .map(item -> item.model)
                .collect(Collectors.toList());
    }

    private static Optional<PropertyModel> getBankAccountModelByBankName(
            ModelList items, BankAccount bankAccount) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(
                        item ->
                                item.type == BANK_ACCOUNT
                                        && item.model
                                                .get(BANK_NAME)
                                                .equals(bankAccount.getBankName()))
                .findFirst()
                .map(item -> item.model);
    }
}
